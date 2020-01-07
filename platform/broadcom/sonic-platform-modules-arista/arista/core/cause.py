import time

from .config import Config
from .inventory import ReloadCause
from .utils import JsonStoredData

RELOAD_CAUSE_HISTORY_SIZE=128

class ReloadCauseEntry(ReloadCause):
   def __init__(self, cause, rcTime='unknown'):
      self.cause = cause
      self.time = rcTime

   def __str__(self):
      return '%s%s' % (self.cause, ', time: %s' % self.time
                       if self.time != "unknown" else '')

   def getCause(self):
      return self.cause

   def getTime(self):
      return self.time

def updateReloadCausesHistory(newCauses):
   rebootCauses = JsonStoredData(Config().reboot_cause_file, lifespan='persistent')
   causes = []
   if rebootCauses.exist():
      causes = rebootCauses.readList(ReloadCauseEntry)
      for newCause in newCauses:
         addCause = True
         for cause in causes:
            if newCause.getTime() == cause.getTime() and \
                  newCause.getCause() == cause.getCause():
               addCause = False
               break
         if addCause:
            causes.append(newCause)
      rebootCauses.clear()
   else:
      causes = newCauses

   if len(causes) > RELOAD_CAUSE_HISTORY_SIZE:
      causes = causes[len(causes) - RELOAD_CAUSE_HISTORY_SIZE:]

   rebootCauses.writeList(causes)

def getReloadCause():
   rebootCauses = JsonStoredData(Config().reboot_cause_file)
   if rebootCauses.exist():
      return rebootCauses.readList(ReloadCauseEntry)
   return None

def getReloadCauseHistory():
   rebootCauses = JsonStoredData(Config().reboot_cause_file, lifespan='persistent')
   if rebootCauses.exist():
      return rebootCauses.readList(ReloadCauseEntry)
   return None

def datetimeToStr(datetime):
   return time.strftime("%Y-%m-%d %H:%M:%S UTC",
                        time.gmtime(time.mktime(datetime.timetuple())))
