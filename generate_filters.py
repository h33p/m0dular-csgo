#!/bin/python
import uuid
import os
import re

abspath = os.path.abspath(__file__)
dname = os.path.dirname(abspath)

builddir = dname + '/build'
addstart = '..\\'

def write_file_group(output, files):
    output += '  <ItemGroup>\n'
    for f in files:
        output += '    <CLCompile Include="' + addstart + f + '">\n'
        output += '      <Filter>' + os.path.dirname(f).replace('/', '\\') + '</Filter>\n'
        output += '    </CLCompile>\n'
    output += '  </ItemGroup>\n'
    return output

def write_include_file_group(output, files):
    output += '  <ItemGroup>\n'
    for f in files:
        output += '    <CLInclude Include="' + addstart + f + '">\n'
        output += '      <Filter>' + os.path.dirname(f).replace('/', '\\') + '</Filter>\n'
        output += '    </CLInclude>\n'
    output += '  </ItemGroup>\n'
    return output

def write_filter_group(output, filters):
    output += '  <ItemGroup>\n'
    for f in filters:
        output += '    <Filter Include="'+f+'">\n'
        output += '      <UniqueIdentifier>{'+str(uuid.uuid4())+'}</UniqueIdentifier>\n'
        output += '    </Filter>\n'
    output += '  </ItemGroup>\n'
    return output

def get_filters_from_dir(dir):
    dirs = dir.split('\\')
    filters = []
    curstring = ''

    for l in dirs:
        if len(curstring):
            curstring += '\\'
        curstring += l
        filters += [curstring]

    return filters

def parse_includes(filename):

    regexp = r'#include \"(.*)\"'

    ret = []

    with open(filename, 'r') as f:
        lines = f.read().splitlines()

        for line in lines:
            so = re.match(regexp, line, re.M|re.I)
            if so and ".h" in so.group(1):
                ret += [so.group(1)]

    return ret

def unrelativize(filename):
    filename = filename.replace('\\', '/')

    dirs = filename.split('/')

    out_dirs = []

    for d in dirs:
        if d == '..':
            del out_dirs[-1]
        else:
            out_dirs += [d]

    out_dir = ""

    for d in out_dirs:
        if out_dir != "":
            out_dir += "/"
        out_dir += d

    return out_dir

def genfilters(dir, file):
    sources = []
    headers = []
    lines = []
    filters = set()

    with open(dir + '/' + file) as f:
        for line in f:
            lines += [line]

    for line in lines:
        if addstart in line:
            filename = re.match(r'(.*((\.h)|(\.cpp)))*', line.split(addstart, 1)[1][:-3], re.M|re.I).group(1)
            if '.cpp' in line:
                sources += [filename]
            elif '.h' in line:
                headers += [filename]

    outputfile = '<?xml version="1.0" encoding="utf-8"?>\n<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n'

    for e in [sources, headers]:
        for filename in e:

            if "\"" in filename:
                print("UWU")
                print(e)

            include_headers = parse_includes(filename)
            filedir = os.path.dirname(''.join(filename))

            for i in include_headers:
                f = unrelativize(os.path.join(filedir, i))
                if os.path.isfile(f) and not f in headers:
                    headers += [f]

    for e in [sources, headers]:
        for filename in e:
            filedir = os.path.dirname(''.join(filename)).replace('/', '\\')
            for filter in get_filters_from_dir(filedir):
                filters.add(filter)

    outputfile = write_file_group(outputfile, sources)
    outputfile = write_include_file_group(outputfile, headers)
    outputfile = write_filter_group(outputfile, filters)

    outputfile += '</Project>\n'

    if len(sources) or len(headers):
        with open(dir + '/' + file + '.filters', 'w') as f:
            f.write(outputfile)
    #print(outputfile)

for f in os.listdir(builddir):
    if str(f).endswith('.vcxproj'):
        genfilters(builddir, f)
