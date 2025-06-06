/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2020 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the 'License');
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an 'AS IS' BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */
//version parallel=false
//version parallel=true,nothor

import ^ as root;
optParallel := #IFDEFINED(root.parallel, false);

#option ('parallelWorkflow', optParallel);
#option('numWorkflowThreads', 5);


import Std.File AS FileServices;
import $.setup;
import Sleep from Std.System.Debug;

prefix := setup.Files(false, false).QueryFilePrefix;
filename := prefix + 'workflow_9bFile';

display(String8 thisString) := FUNCTION
  ds := dataset([thisString], {String8 text});
  RETURN Output(ds, NAMED('logging'), EXTEND);
END;
Import sleep from std.System.Debug;

//This makes sure that item B is executed twice because it is **not** independent
names := RECORD
  STRING8 firstname;
  STRING8 surname;
END;

ds := DATASET([{'nathan', 'halliday'}, {'margaret', 'thatcher'}], names);

A := OUTPUT(ds,,filename, OVERwrite) : independent;
A2 := OUTPUT(ds+ds,,filename, OVERwrite) : independent;

MyFile := DATASET(filename, names, thor);
B := COUNT(myFile);

conditionalDelete(string lfn) := FUNCTION
  RETURN IF(FileServices.FileExists(lfn), FileServices.DeleteLogicalFile(lfn));
END;
SEQUENTIAL(
  conditionalDelete(filename),
  SEQUENTIAL(A,B,
    // Roxie caches files by crc and modification date
    // Only 1 second granularity can be guaranteed on the modification dates - so sleep to ensure the update is spotted
    Sleep(1000),
    A2,B),
  FileServices.DeleteLogicalFile(filename)
);
