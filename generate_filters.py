#!/bin/python
import uuid
import os

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
            filename = [line.split(addstart, 1)[1][:-3]]
            filedir = os.path.dirname(''.join(filename)).replace('/', '\\')
            if '.cpp' in line:
                sources += filename
                for filter in get_filters_from_dir(filedir):
                    filters.add(filter)
            elif '.h' in line:
                headers += [''.join(filename)[:-1]]
                for filter in get_filters_from_dir(filedir):
                    filters.add(filter)

    outputfile = '<?xml version="1.0" encoding="utf-8"?>\n<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n'

    outputfile = write_file_group(outputfile, sources)
    outputfile = write_file_group(outputfile, headers)
    outputfile = write_filter_group(outputfile, filters)

    outputfile += '</Project>\n'

    if len(sources) or len(headers):
        with open(dir + '/' + file + '.filters', 'w') as f:
            f.write(outputfile)
    #print(outputfile)

for f in os.listdir(builddir):
    if str(f).endswith('.vcxproj'):
        genfilters(builddir, f)
