//******************************************************************************************************
//  CommonTypes.cpp - Gbtc
//
//  Copyright � 2018, Grid Protection Alliance.  All Rights Reserved.
//
//  Licensed to the Grid Protection Alliance (GPA) under one or more contributor license agreements. See
//  the NOTICE file distributed with this work for additional information regarding copyright ownership.
//  The GPA licenses this file to you under the MIT License (MIT), the "License"; you may
//  not use this file except in compliance with the License. You may obtain a copy of the License at:
//
//      http://opensource.org/licenses/MIT
//
//  Unless agreed to in writing, the subject software distributed under the License is distributed on an
//  "AS-IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. Refer to the
//  License for the specific language governing permissions and limitations.
//
//  Code Modification History:
//  ----------------------------------------------------------------------------------------------------
//  03/23/2018 - J. Ritchie Carroll
//       Generated original version of source code.
//
//******************************************************************************************************

#include "CommonTypes.h"
#include "Convert.h"

using namespace std;
using namespace GSF::TimeSeries;

const string Empty::String;

const Guid Empty::Guid = ToGuid("00000000-0000-0000-0000-000000000000");

const IPAddress Empty::IPAddress;

const uint8_t* Empty::ZeroLengthBytes = new uint8_t[4] { 0, 0, 0, 0 };