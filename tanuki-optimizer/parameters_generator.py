#!/usr/bin/python
# -*- coding: utf-8 -*-
import codecs
import math
import re

def print_value(min_value, max_value, name, default_value):
    min_value = int(eval(min_value))
    max_value = int(eval(max_value))
    default_value = int(eval(default_value))

    # hp.quniform('PARAM_FUTILITY_MARGIN_ALPHA_ENDING', 112, 188, 1),
    #print("    hp.quniform('{name}', {min_value}, {max_value}, 1),".format(
    #    name=name, min_value=min_value, max_value=max_value))

    # 'PARAM_FUTILITY_MARGIN_ALPHA_ENDING',
    #print("    '{name}',".format(name=name))

    # name,default_value,min_value,max_value
    print("{name},{default_value},{min_value},{max_value}".format(
        name=name, default_value=default_value, min_value=min_value, max_value=max_value))


def main():
    with codecs.open('param/2018-otafuku-param.original.h', 'r', 'utf-8') as f:
        min_value = None
        max_value = None
        name = None
        default_value = None
        for line in f:
            for m in re.finditer('min:(.+?),max:(.+?),', line):
                if min_value or max_value:
                    raise Exception('multiple min_value and max_value were found...')
                min_value = m.group(1)
                max_value = m.group(2)

            for m in re.finditer('PARAM_DEFINE (.+?) = (.+?);', line):
                if not min_value and not_max_value:
                    raise Exception('min_value and max_value were not found...')
                if name and default_value:
                    raise Exception('multiple name and default_value were found...')
                name = m.group(1)
                default_value = m.group(2)

            if (min_value or max_value) and name:
                if name not in ['PARAM_RAZORING_MARGIN1', 'PARAM_QSEARCH_MATE1', 'PARAM_SEARCH_MATE1', 'PARAM_WEAK_MATE_PLY']:
                    print_value(min_value, max_value, name, default_value)
                min_value = None
                max_value = None
                name = None
                default_value = None


if __name__ == '__main__':
    main()
