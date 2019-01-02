#!/usr/bin/python2
import unittest
import os


if __name__ == '__main__':
    testdir = os.path.dirname(os.path.realpath(__file__))
    testsuite = unittest.TestLoader().discover(testdir)
    unittest.TextTestRunner(verbosity=1).run(testsuite)
