#!/usr/bin/python
# -*- coding: utf-8 -*-

import csv

def main():
  with open('param/parameters.csv', 'r') as f:
    reader = csv.reader(f)
    for index, row in enumerate(reader):
      name, default_value, min_value, max_value = row
      #print("{0}={1}".format(name, default_value))
      #print('-D{0}=$({0})'.format(name))
      #print("  hp.quniform('{0}', {1}, {2}, 1),".format(name, min_value, max_value))
      #print("    '{0}=' + str(int(args[{1}])),".format(name, index))
      #print('#ifndef {0}'.format(name))
      #print('#define {0} {1}'.format(name, str(int(parameters[index]))))
      #print('#endif')
      #print("{0}={1}".format(name, str(int(parameters[index]))))
      #print("  '{0}',".format(name))
      if name.endswith('_OPENING'):
        sigma = (float(max_value) - float(min_value)) / 6.0
      else:
        sigma = 1e-8
      print("  hp.qnormal('{0}', {1}, {2}, 1),".format(name, default_value, sigma))

if __name__ == '__main__':
    main()
