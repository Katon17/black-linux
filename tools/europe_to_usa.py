# black-linux: the Europe release's default.xbe turned into the USA one with src/common/europe.patch, the same way
# desktop/game_files.cpp (ConvertEurope) does it at run time.
#   europe_to_usa.py <europe.patch> <europe default.xbe> <out usa default.xbe>
import sys


class Reader:
    def __init__(self, data):
        self.data, self.at, self.failed = data, 0, False

    def done(self):
        return self.at == len(self.data)

    def byte(self):
        if self.at == len(self.data):
            self.failed = True
            return 0
        b = self.data[self.at]
        self.at += 1
        return b

    # 7 bits a byte, low first
    def number(self):
        n, shift = 0, 0
        while shift < 64:
            b = self.byte()
            n |= (b & 0x7F) << shift
            if b < 0x80:
                return n
            shift += 7
        self.failed = True
        return 0


def convert(patch, src):
    p, read, out = Reader(patch), 0, bytearray()
    while not p.done():
        copy = p.number()
        if read > len(src) or copy > len(src) - read:
            raise ValueError("patch does not fit this xbe")
        start = len(out)
        out += src[read:read + copy]
        at = 0
        for _ in range(p.number()):
            if p.failed:
                break
            at += p.number()
            if at >= copy:
                raise ValueError("patch change past its block")
            out[start + at] = (out[start + at] + p.byte()) & 0xFF
            at += 1
        for _ in range(p.number()):
            if p.failed:
                break
            out.append(p.byte())
        end = read + copy
        move = p.number()
        distance = move >> 1
        if distance > len(src):
            raise ValueError("patch move out of range")
        if not (move & 1):
            read = end + distance
        else:
            if distance >= end:
                raise ValueError("patch move out of range")
            read = end - distance - 1
        if p.failed:
            raise ValueError("patch cut short")
    return bytes(out)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("usage: europe_to_usa.py <europe.patch> <europe default.xbe> <out>")
    patch = open(sys.argv[1], "rb").read()
    src = open(sys.argv[2], "rb").read()
    try:
        out = convert(patch, src)
    except ValueError as e:
        sys.exit(str(e))
    open(sys.argv[3], "wb").write(out)
