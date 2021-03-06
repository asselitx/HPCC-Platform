################################################################################
#    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
################################################################################

# Component: backupnode 
#####################################################
# Description:
# ------------
#    Cmake Input File for backupnode
#####################################################

project( backupnode ) 

set (    SRCS 
         backupnode.cpp 
         backupnode2.cpp 
    )

include_directories ( 
         ./../../system/include 
         ./../../system/jlib 
         ./../../system/mp
         ./../../fs/dafsclient
         ./../../dali/base
         ./../../system/jhtree
         ./../../rtl/include
         ./../../rtl/eclrtl
         ./../../system/security/shared
    )

ADD_DEFINITIONS( -D_CONSOLE )

HPCC_ADD_EXECUTABLE ( backupnode ${SRCS} )
install ( TARGETS backupnode RUNTIME DESTINATION ${EXEC_DIR} )

target_link_libraries (  backupnode 
         jlib
         jhtree
         dafsclient
         mp
         dalibase
    )


