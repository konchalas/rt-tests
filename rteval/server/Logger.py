import sys
from datetime import datetime

class Logger():
    def __init__(self, logfile, prefix):
        self.log = sys.stdout
        self.logopen = True
        self.prefix = prefix

    def Log(self, grp, msg):
        if self.logopen == True:
            tstmp = datetime.today().strftime("%Y-%m-%d %H:%M:%S")
            self.log.write("%s [%s::%s]: %s\n" % (tstmp, self.prefix, grp, msg))
            self.log.flush()

    def LogFD(self):
        return self.log.fileno()

