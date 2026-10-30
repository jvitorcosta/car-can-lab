"""Which IDs the adapter passes on while it monitors: the firmware's src/util/sweep in Python.

A window passes the IDs where (id & mask) == filter. The plan cycles through windows of the 11-bit
ID space; one that overflows is split in two.
"""
ALL = 0x7FF
START_SIZE = 0x80
MAX_WINDOWS = 128


class Plan:
    def __init__(self):
        self.scan()

    def scan(self):
        self.windows = [(f, ALL & ~(START_SIZE - 1)) for f in range(0, ALL + 1, START_SIZE)]
        self.cur = 0

    def only(self, ids):
        """Only these IDs. Aligned blocks of 2, 4 or 8 share a window when at least half is wanted."""
        want = {i & ALL for i in ids}
        if not want:
            self.scan()
            return
        self.windows, self.cur = [], 0
        done = set()
        for cid in sorted(want):
            if cid in done or len(self.windows) == MAX_WINDOWS:
                continue
            size = 1
            for b in (8, 4, 2):
                base = cid & ~(b - 1)
                k = sum(1 for j in range(base, base + b) if j in want and j not in done)
                if k >= 2 and 2 * k >= b:
                    size = b
                    break
            base = cid & ~(size - 1)
            done.update(range(base, base + size))
            self.windows.append((base, ALL & ~(size - 1)))

    def current(self):
        return self.windows[self.cur]

    def next(self):
        """Moves on; True when that went back to the first window."""
        self.cur = (self.cur + 1) % len(self.windows)
        return self.cur == 0

    def split(self):
        """The current window becomes its two halves, current on the first; False when it can't."""
        filt, mask = self.current()
        if mask == ALL or len(self.windows) >= MAX_WINDOWS:
            return False
        half = ((~mask & ALL) + 1) // 2
        self.windows[self.cur:self.cur + 1] = [(filt, mask | half), (filt | half, mask | half)]
        return True
