#!/usr/bin/env python
from __future__ import absolute_import

import logging
import sys

def getLogger(name):
   logger = logging.getLogger(name)
   logger.setLevel(logging.DEBUG)
   logOut = logging.StreamHandler(sys.stdout)
   logOut.setFormatter(logging.Formatter('%(asctime)-15s %(levelname)s: '
                                         '%(message)s'))
   logOut.setLevel(logging.DEBUG)
   logger.addHandler(logOut)
   logger.propagate = 0
   return logger
