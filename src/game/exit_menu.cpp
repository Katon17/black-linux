// Quit on the main menu, as the SCDA recomp has: a QUIT button under the title menu's own opens the game's
// ConfirmWarning page with its unused "are you sure you want to quit?" text. Yes closes the app, No goes back.
// CREDITS between the title menu's own and QUIT opens the options page as the credits (options_menu.cpp).
// Back on the title menu, which did nothing on the console, opens the same page. The page is the pause menu's and
// imports its sounds from PAudio, so on the front end its sound clips take the title menu's FEAudio ones.
#include <cstring>
#include <iterator>
#include <string>

#include <strings.h>

#include <SDL3/SDL.h>

#include "app/app.h"
#include "common/translations.h"
#include "core/log.h"
#include "game/flash.h"
#include "game/layout.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace layout;
using flash::kClipData;
using flash::kDataCharacter;
using flash::kSprite;
using flash::Member;

namespace game {
void OpenCredits(const char* title_page);  // options_menu.cpp
}

namespace {

// ConfirmWarning's title and message, text bindings as SetupConfirmWarning (0x11A660) fills them
constexpr uint32_t kConfirmTitle = 0x2EC600;
constexpr uint32_t kConfirmMessage = 0x2EC8F8;
constexpr uint32_t kSetBindingText = 0x11A4A0;  // eax binding, ebx UTF-8 text
constexpr uint32_t kStrings = 0x2D45D8;
constexpr uint32_t kLookup = 0x1F3A80;   // thiscall strings (string id): UTF-8 text, 0 before they load
constexpr uint32_t kQuitId = 0x4A8272D7;         // FE_QUIT
constexpr uint32_t kQuitQuestionId = 0x50756353;  // "are you sure you want to quit?"

// character and its movie: imports {path, name, character, loaded movie}.
// A loaded movie +0 counts the imports holding it; at none it is freed and its characters unrelocated (0xFC360).
constexpr uint32_t kCharacterMovie = 0x04;
constexpr uint32_t kImportCount = 0x28;
constexpr uint32_t kImports = 0x2C;
constexpr uint32_t kImportSize = 0x10;
constexpr uint32_t kImportName = 0x04;
constexpr uint32_t kImportLoaded = 0x0C;
constexpr uint32_t kMaxImports = 256;

constexpr char kConfirmPage[] = "ConfirmWarning";
constexpr char kQuitText[] = "$FE_QUIT";
constexpr char kButtonName[] = "QuitButton";
constexpr char kButtonDepth[] = "100";
constexpr char kCreditsName[] = "CreditsButton";
constexpr char kCreditsDepth[] = "101";
constexpr char kCreditsText[] = "$RC_CREDITS";  // a string of ours, game::MenuText
constexpr char kOptionsPage[] = "OptionsMenuXbox";
constexpr float kPitch = 25.0f;  // between the stock buttons

struct TitleMenu {
    const char* page;
    const char* last_clip;  // in Main.TextAnim, ours goes under it
    const char* first;      // the ring's first and last button objects
    const char* last;
};
// as the game loads them (0x1F8BF2, 0x1F8C04); longest name first, they are compared by prefix
constexpr TitleMenu kTitleMenus[] = {
    {"blacktitlemenu2", "options_text", "startButton", "optionsButton"},
    {"blacktitlemenu", "operatorprofile_text", "continueButton", "profileButton"},
};

// the root sound clips every page script plays
constexpr const char* kSoundClips[] = {"SNDBack", "SNDChange", "SNDClose", "SNDError", "SNDOpen", "SNDSelect"};
constexpr size_t kSounds = std::size(kSoundClips);

const TitleMenu* g_return = &kTitleMenus[1];  // the title menu No goes back to
bool g_confirm_ready = false;  // the page's No and back lead there
bool g_quitting = false;
bool g_reported = false;  // a failure is logged once
uint32_t g_title_sounds[kSounds] = {};  // the characters of the title menu's sound clips
uint32_t g_held_audio = 0;  // FEAudio, held loaded while ConfirmWarning plays its clips

// page names differ in case between the game and the scripts
bool HasPrefix(const char* text, const char* prefix) { return strncasecmp(text, prefix, std::strlen(prefix)) == 0; }

const TitleMenu* FindTitleMenu(const char* page) {
    for (const TitleMenu& menu : kTitleMenus)
        if (HasPrefix(page, menu.page)) return &menu;
    return nullptr;
}

bool InFrontEnd() {
    uint32_t game = R32(kGame);
    return game && R32(game + kGameState) == game + kFrontEnd;
}

void SetConfirmText(X86Context& ctx, uint32_t binding, uint32_t id) {
    ctx.ecx = R32(kStrings);
    uint32_t text = CallGuest(ctx, kLookup, {id});
    ctx.eax = binding;
    ctx.ebx = text;
    CallGuest(ctx, kSetBindingText, {});
}

// a stopped copy of the last stock button
uint32_t CopyButton(X86Context& ctx, const TitleMenu& title, uint32_t holder, const char* name, const char* depth) {
    std::string path = std::string("_level4.Main.TextAnim.") + title.last_clip;
    uint32_t function = flash::Text("duplicateMovieClip");
    ctx.eax = flash::Text(path.c_str());
    CallGuest(ctx, flash::kCall, {function, 0, 2, flash::Text(name), flash::Text(depth)});
    uint32_t clip = Member(ctx, holder, name);
    if (flash::Type(clip) != kSprite) return 0;
    uint32_t data = R32(clip + kClipData);
    W32(data + flash::kDataFlags, R32(data + flash::kDataFlags) & ~flash::kDataPlaying);
    return clip;
}

// it keeps its place under the last stock button, which the page fades in, rows down
void Follow(uint32_t button, uint32_t lead, int rows = 1) {
    G<float>(button + flash::kClipX) = G<float>(lead + flash::kClipX);
    G<float>(button + flash::kClipY) = G<float>(lead + flash::kClipY) + kPitch * float(rows);
    std::memcpy(HostPtr(button + flash::kClipColours), HostPtr(lead + flash::kClipColours), flash::kColoursSize);
}

// the button joins the ring and asks the question; back asks it too
bool JoinRing(X86Context& ctx, const TitleMenu& title, uint32_t root, uint32_t button) {
    uint32_t first = Member(ctx, root, title.first);
    uint32_t last = Member(ctx, root, title.last);
    uint32_t menu = Member(ctx, root, "MyMenu");
    if (!first || !last || !menu || !flash::Button(ctx, button, first)) return false;
    if (!flash::Set(ctx, button, "actionSelectPage", flash::String(ctx, kConfirmPage))) return false;
    flash::Link(ctx, last, button);
    flash::Link(ctx, button, first);
    return flash::Set(ctx, menu, "mstrPageBack", flash::String(ctx, kConfirmPage));
}

// the step that failed, null once the button is in
const char* AddButton(X86Context& ctx, const TitleMenu& title, uint32_t root, uint32_t holder, uint32_t lead) {
    uint32_t button = CopyButton(ctx, title, holder, kButtonName, kButtonDepth);
    if (!button) return "copy";
    uint32_t field = flash::FindField(button, nullptr);
    if (!field || !flash::Set(ctx, field, "variable", flash::String(ctx, kQuitText))) return "label";
    Follow(button, lead, 2);
    CallGuest(ctx, flash::kRunActions, {R32(flash::kPlayer)});
    return JoinRing(ctx, title, root, button) ? nullptr : "ring";
}

// CREDITS between the last stock button and QUIT, in the ring too; it goes to the options page, which UpdateCredits
// has load as the credits
const char* AddCredits(X86Context& ctx, const TitleMenu& title, uint32_t root, uint32_t holder, uint32_t lead,
                       uint32_t quit) {
    uint32_t button = CopyButton(ctx, title, holder, kCreditsName, kCreditsDepth);
    if (!button) return "credits copy";
    uint32_t field = flash::FindField(button, nullptr);
    if (!field || !flash::Set(ctx, field, "variable", flash::String(ctx, kCreditsText))) return "credits label";
    Follow(button, lead, 1);
    CallGuest(ctx, flash::kRunActions, {R32(flash::kPlayer)});
    uint32_t first = Member(ctx, root, title.first);
    uint32_t last = Member(ctx, root, title.last);
    if (!first || !last || !flash::Button(ctx, button, first)) return "credits ring";
    if (!flash::Set(ctx, button, "actionSelectPage", flash::String(ctx, kOptionsPage))) return "credits page";
    flash::Link(ctx, last, button);
    flash::Link(ctx, button, quit);
    return nullptr;
}

void ReportOnce(const char* page, const char* what) {
    if (g_reported) return;
    g_reported = true;
    LOGE("quit: %s, %s failed", page, what);
}

void UpdateButton(X86Context& ctx, uint32_t ui, const TitleMenu& title, uint32_t root) {
    uint32_t holder = Member(ctx, Member(ctx, root, "Main"), "TextAnim");
    uint32_t lead = Member(ctx, holder, title.last_clip);
    // none until Main places TextAnim
    if (!flash::IsClip(lead)) return;
    uint32_t quit = Member(ctx, holder, kButtonName);
    uint32_t credits = Member(ctx, holder, kCreditsName);
    if (quit) Follow(quit, lead, 2);
    if (credits) Follow(credits, lead, 1);
    if ((quit && credits) || R32(ui + kPageLeaving) || R32(ui + kPageAsked) == 1) return;
    const char* failed = quit ? nullptr : AddButton(ctx, title, root, holder, lead);
    if (!failed && !(quit = Member(ctx, holder, kButtonName))) failed = "quit lookup";
    if (!failed) failed = AddCredits(ctx, title, root, holder, lead, quit);
    if (failed) {
        ReportOnce(title.page, failed);
    } else {
        LOGI("quit: buttons added to %s", title.page);
    }
}

// a confirm on CREDITS: the options page it goes to loads as the credits, back to this title menu
void UpdateCredits(X86Context& ctx, uint32_t ui, const TitleMenu& title, uint32_t root) {
    if (R32(ui + kPageLeaving) || R32(ui + kPageAsked) == 1 || !InputTaken(ui)) return;
    if (R32(ui + kMenuInput) != kInputConfirm) return;
    uint32_t credits = Member(ctx, Member(ctx, Member(ctx, root, "Main"), "TextAnim"), kCreditsName);
    uint32_t focus = Member(ctx, Member(ctx, root, "MyMenu"), "mFocusButton");
    if (credits && focus == credits) game::OpenCredits(title.page);
}

uint32_t ClipCharacter(X86Context& ctx, uint32_t root, const char* name) {
    uint32_t clip = Member(ctx, root, name);
    if (flash::Type(clip) != kSprite) return 0;
    return R32(R32(clip + kClipData) + kDataCharacter);
}

// the loaded FEAudio the title movie's sound clips are imported from, found through its Main clip's character
uint32_t TitleAudio(X86Context& ctx, uint32_t root) {
    uint32_t main = ClipCharacter(ctx, root, "Main");
    uint32_t movie = main ? R32(main + kCharacterMovie) : 0;
    uint32_t count = movie ? R32(movie + kImportCount) : 0;
    for (uint32_t i = 0; i < count && i < kMaxImports; i++) {
        uint32_t entry = R32(movie + kImports) + i * kImportSize;
        const char* name = static_cast<const char*>(HostPtr(R32(entry + kImportName)));
        if (strcasecmp(name, kSoundClips[0]) == 0) return R32(entry + kImportLoaded);
    }
    return 0;
}

// one more import's hold keeps FEAudio loaded after the title movie lets go of it
void LatchTitleSounds(X86Context& ctx, uint32_t root) {
    for (size_t i = 0; i < kSounds; i++) g_title_sounds[i] = ClipCharacter(ctx, root, kSoundClips[i]);
    if (g_held_audio) return;
    g_held_audio = TitleAudio(ctx, root);
    if (g_held_audio) W32(g_held_audio, R32(g_held_audio) + 1);
}

// back on a title menu, which holds FEAudio again, so ours never frees it
void ReleaseTitleAudio() {
    if (!g_held_audio || R32(g_held_audio) < 2) return;
    W32(g_held_audio, R32(g_held_audio) - 1);
    g_held_audio = 0;
}

// only the front end page's own clips change, the pause menu's ConfirmWarning makes new ones with PAudio's
void ApplyTitleSounds(X86Context& ctx, uint32_t root) {
    if (!g_held_audio) {
        ReportOnce(kConfirmPage, "FEAudio");
        return;
    }
    for (size_t i = 0; i < kSounds; i++) {
        uint32_t clip = Member(ctx, root, kSoundClips[i]);
        if (flash::Type(clip) != kSprite || !g_title_sounds[i]) {
            ReportOnce(kConfirmPage, kSoundClips[i]);
            continue;
        }
        W32(R32(clip + kClipData) + kDataCharacter, g_title_sounds[i]);
    }
}

// as the question is asked, before the page loads and reads its text
void ApplyQuestion(X86Context& ctx, uint32_t ui, const TitleMenu& title, uint32_t root) {
    const char* target = static_cast<const char*>(HostPtr(ui + kTargetPage));
    if (R32(ui + kPageAsked) != 1 || !HasPrefix(target, kConfirmPage)) return;
    SetConfirmText(ctx, kConfirmTitle, kQuitId);
    SetConfirmText(ctx, kConfirmMessage, kQuitQuestionId);
    LatchTitleSounds(ctx, root);
    g_return = &title;
}

// No and back lead to the title menu; the page took them from what the last pause menu left, or none
bool PointBack(X86Context& ctx, uint32_t root) {
    uint32_t menu = Member(ctx, root, "MyMenu");
    uint32_t no = Member(ctx, root, "noButton");
    if (!menu || !no) return false;
    return flash::Set(ctx, no, "actionSelectPage", flash::String(ctx, g_return->page)) &&
           flash::Set(ctx, menu, "mstrPageBack", flash::String(ctx, g_return->page));
}

// Yes is answered here and its input taken, so the game code a pause menu left on it never runs
void UpdateConfirm(X86Context& ctx, uint32_t ui, uint32_t root) {
    if (!g_confirm_ready) g_confirm_ready = PointBack(ctx, root);
    if (R32(ui + kPageLeaving) || R32(ui + kPageAsked) == 1 || !InputTaken(ui)) return;
    if (R32(ui + kMenuInput) != kInputConfirm) return;
    uint32_t yes = Member(ctx, root, "yesButton");
    uint32_t focus = Member(ctx, Member(ctx, root, "MyMenu"), "mFocusButton");
    if (!yes || focus != yes) return;
    W32(ui + kMenuInput, 0);
    if (g_quitting) return;
    g_quitting = true;
    LOGI("quit: closing the app");
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit);
}

void Tick(X86Context& ctx, uint32_t ui) {
    const char* page = static_cast<const char*>(HostPtr(ui + kPageName));
    bool confirm = HasPrefix(page, kConfirmPage) && InFrontEnd();
    if (!confirm) g_confirm_ready = false;
    if (R32(flash::kPlayerReady) != 1 || !R32(flash::kPlayer) || R32(ui + kPageReady) != kPageRunning) return;
    uint32_t root = flash::Level(flash::kMenuLevel);
    if (!root) return;
    if (const TitleMenu* title = FindTitleMenu(page)) {
        ReleaseTitleAudio();
        UpdateButton(ctx, ui, *title, root);
        UpdateCredits(ctx, ui, *title, root);
        ApplyQuestion(ctx, ui, *title, root);
    }
    if (confirm) UpdateConfirm(ctx, ui, root);
}

}  // namespace

namespace game {

// prompts.cpp's string lookup: CREDITS's text, 0 for any other id
uint32_t MenuText(uint32_t id) {
    static const uint32_t kId = flash::StringId(kCreditsText + 1);
    static uint32_t text = 0;
    if (id != kId) return 0;
    if (!text) {
        std::string credits = app::Ui("CREDITS");
        text = mem::AllocSystem(uint32_t(credits.size() + 1));
        std::memcpy(HostPtr(text), credits.c_str(), credits.size() + 1);
    }
    return text;
}

}  // namespace game

// ebp the front end, after the cursor's input and before the Flash player runs the page's script
void ExitMenu(X86Context& ctx, uint8_t*) {
    X86Context saved = ctx;
    Tick(ctx, ctx.ebp);
    ctx = saved;
}

// ebp the front end, a page just loaded and its Prepare, which plays SNDOpen, not yet called
void ExitMenuSounds(X86Context& ctx, uint8_t*) {
    X86Context saved = ctx;
    const char* page = static_cast<const char*>(HostPtr(ctx.ebp + kPageName));
    uint32_t root = flash::Level(flash::kMenuLevel);
    if (root && HasPrefix(page, kConfirmPage) && InFrontEnd()) ApplyTitleSounds(ctx, root);
    ctx = saved;
}
