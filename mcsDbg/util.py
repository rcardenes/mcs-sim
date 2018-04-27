# vim: ai:sw=4:sts=4:expandtab
from datetime import datetime, timedelta
from time import mktime
import numpy as np

def get_datetime(text):
    try:
        return datetime.strptime(text, "%m/%d/%Y %H:%M:%S.%f")
    except ValueError:
        return datetime.strptime(text[:-3], "%m/%d/%Y %H:%M:%S.%f")

def get_split_stamp(dt):
    return mktime(dt.timetuple()), dt.microsecond

class CumulativeAverage(object):
    def __init__(self):
        self.n = 0
        self.avg = 0.

    def add(self, x):
        self.avg = self.avg + (x - self.avg) / (self.n + 1)
        self.n += 1

import pdb

def generate_time_series(dt1, dt2, n, cma=None, to_datetime=True, reverse=False, UTC=False):
    t1, mst1 = get_split_stamp(dt1)
    t2, mst2 = get_split_stamp(dt2)
    t1 = t1 * 1000000 + mst1
    t2 = t2 * 1000000 + mst2
    delta = (t2 - t1) / n
    if cma:
        cma.add(delta / 1000000)
    fn = lambda x: x
    if to_datetime:
        fn = datetime.utcfromtimestamp if UTC else datetime.fromtimestamp
    series = [fn(x) for x in np.arange(t1, t2, delta) / 1000000]
    if reverse:
        series.reverse()

    return series

class CsvFile(object):
    def __init__(self, fobj, cols):
        # Make sure that we're at the beginning of the file, and discard the first 4 lines (header)
        # "Cols" is the number of valid data columns, excluding the timestamp AND possible "Repeat" instances
        self.cols = cols + 1
        fobj.seek(0)
        fobj.readline()
        fobj.readline()
        fobj.readline()
        fobj.readline()
        self.fobj = fobj

    def __iter__(self):
        def data_tuples(f):
            for line in f:
                fields = line.strip().split('\t')
                yield (get_datetime(fields[0]),) + tuple((float(x) for x in fields[1:self.cols])) + tuple(fields[self.cols:])

        cma_delta = CumulativeAverage()
        try:
            source = data_tuples(self.fobj)
            while True:
                fields = source.next()
                if len(fields) == self.cols:
                    yield fields
                elif "Repeat" in fields[-1]:
                    stack = []
                    dt = fields[0]
                    total_elements = int(fields[-1].split()[-1])
                    yield fields[:-1]
                    cols = fields[1:-1]

                    repeating = True
                    while repeating:
                        try:
                            next_fields = source.next()
                            next_dt = next_fields[0]
                            if len(next_fields) == self.cols:
                                repeating = False
                            else:
                                raise ValueError("Corrupt data at {0}".format(next_fields[0]))

                            stack = [(x,) + cols for x in [next_dt] + generate_time_series(dt, next_dt, n=total_elements, cma=cma_delta, reverse=True)[:-1]] + stack

                            if repeating:
                                dt = next_dt
                                total_elements = int(next_fields[-1].split()[-1])
                        except StopIteration:
                            delta = timedelta(seconds=cma_delta.avg * total_elements)
                            stack = [(x,) + cols for x in generate_time_series(dt, dt + delta, total_elements)[:-1]] + stack
                            break
                    while stack:
                        yield stack.pop()
                else:
                    raise ValueError("Corrupt data at {0}".format(fields[0]))

        except StopIteration:
            pass


if __name__ == '__main__':
    fname = 'test_data/azCurrentMaxAcc'
    c = CsvFile(open(fname), 1)
    i = iter(c)
