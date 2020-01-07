
from collections import namedtuple
import logging
import os
import time

from ..core.component import Component, DEFAULT_WAIT_TIMEOUT, ASIC_YIELD_TIME
from ..core.utils import klog, inSimulation

from ..drivers.i2c import I2cKernelDriver

SensorDesc = namedtuple('SensorDesc', [
   'diode',
   'name',
   'position',
   'target',
   'overheat',
   'critical',
])

class PciComponent(Component):
   def __init__(self, **kwargs):
      super(PciComponent, self).__init__(**kwargs)

class I2cComponent(Component):
   def __init__(self, **kwargs):
      super(I2cComponent, self).__init__(**kwargs)

   def __str__(self):
      return '%s(addr=%s)' % (self.__class__.__name__, self.addr)

# Do not use this class as it is being depreciated
class I2cKernelComponent(I2cComponent):
   def __init__(self, addr, name, waitFile=None, waitTimeout=None, **kwargs):
      drivers = [I2cKernelDriver(name=name, addr=addr, waitFile=waitFile,
                                 waitTimeout=waitTimeout)]
      super(I2cKernelComponent, self).__init__(addr=addr, name=name,
                                               drivers=drivers, **kwargs)

class SwitchChip(PciComponent):
   def __init__(self, addr, **kwargs):
      super(SwitchChip, self).__init__(addr=addr, **kwargs)

   def __str__(self):
      return '%s(addr=%s)' % (self.__class__.__name__, self.addr)

   def pciRescan(self):
      logging.info('triggering kernel pci rescan')
      with open('/sys/bus/pci/rescan', 'w') as f:
         f.write('1\n')

   def waitForIt(self, timeout=DEFAULT_WAIT_TIMEOUT):
      begin = time.time()
      end = begin + timeout
      rescanTime = begin + (timeout / 2)
      devPath = self.addr.getSysfsPath()

      logging.debug('waiting for switch chip %s', devPath)
      if inSimulation():
         return True

      klog('waiting for switch chip')
      while True:
         now = time.time()
         if now > end:
            break
         if os.path.exists(devPath):
            logging.debug('switch chip is ready')
            klog('switch chip is ready')
            time.sleep(ASIC_YIELD_TIME)
            klog('yielding...')
            return True
         if now > rescanTime:
            self.pciRescan()
            rescanTime = end
         time.sleep(0.1)

      logging.error('timed out waiting for the switch chip %s', devPath)
      return False
