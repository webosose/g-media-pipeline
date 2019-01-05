#!/bin/sh

# Copyright (c) 2018-2019 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

DOT_FILES_DIR=$1

if test -d $DOT_FILES_DIR
then
    DOT_FILES=`ls --ignore="*.sh" $DOT_FILES_DIR | grep dot`

    for dot_file in $DOT_FILES
    do
        echo "Generate the PNG from the $dot_file"
        png_file=`echo $dot_file | sed s/.dot/.png/`
        dot -Tpng $DOT_FILES_DIR/$dot_file > $DOT_FILES_DIR/$png_file
    done
else
    echo "No search file and directory in [$DOT_FILES_DIR]"
    echo "Usage : ./convert_dot.sh [dot files directory]"
fi

