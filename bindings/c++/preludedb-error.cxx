/*****
*
* Copyright (C) 2014 CS-SI. All Rights Reserved.
* Author: Yoann Vandoorselaere <yoann@prelude-ids.com>
*
* This file is part of the Prelude library.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#include <string>
#include <iostream>

#include "preludedb-error.hxx"
#include <preludedb.h>


using namespace PreludeDB;


PreludeDBError::PreludeDBError(const std::string &message) throw()
{
        _error = -1;
        _message = message;
}


PreludeDBError::PreludeDBError(int error) throw()
{
        _error = error;
        _message = std::string(preludedb_strerror(error));
}
