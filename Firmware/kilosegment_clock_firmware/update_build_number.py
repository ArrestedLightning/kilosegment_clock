#Generate a header file which contains a unique build number and a timestamp for inclusion in the firmware build
import re
import datetime

#strings
FILE_NAME = "./src/build_info.h"
BUILD_NUMBER_DEF = "#define BUILD_NUMBER "
BUILD_DATE_DEF = "#define BUILD_DATE "
FILE_HEADER = "#ifndef _BUILD_INFO_H_\n#define _BUILD_INFO_H_\n\n"
FILE_FOOTER = "\n\n#endif"

#get the current time in YYYY-MM-DD HH:MM:SS format
build_date = timenow = str(datetime.datetime.now().strftime("%Y-%m-%d %H.%M.%S"))[:19]

build_number = 100 #default if nothing found

with open(FILE_NAME, "r") as f:
    f_contents = f.read()
    #pull the existing build number out of the header file using regex
    bn = re.search(BUILD_NUMBER_DEF + "([0-9]+)", f_contents)
    if bn:
        build_number = int(bn.group(1)) + 1

print("Build Number: {}".format(build_number))

#write updated file
with open(FILE_NAME, "w") as f:
    f.write(FILE_HEADER)
    f.write(BUILD_NUMBER_DEF + str(build_number) + "\n")
    f.write(BUILD_DATE_DEF + "\"" + build_date + "\"")
    f.write(FILE_FOOTER)