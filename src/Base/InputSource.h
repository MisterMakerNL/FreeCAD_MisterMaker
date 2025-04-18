/***************************************************************************
 *   Copyright (c) 2011 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#ifndef BASE_IINPUTSOURCE_H
#define BASE_IINPUTSOURCE_H

#include <iosfwd>
#include <memory>

#include <xercesc/util/BinInputStream.hpp>
#include <xercesc/sax/InputSource.hpp>
#ifndef FC_GLOBAL_H
#include <FCGlobal.h>
#endif

#ifndef XERCES_CPP_NAMESPACE_BEGIN
#define XERCES_CPP_NAMESPACE_QUALIFIER
namespace XERCES_CPP_NAMESPACE
{
class BinInputStream;
}
#else
XERCES_CPP_NAMESPACE_BEGIN
class BinInputStream;
XERCES_CPP_NAMESPACE_END
#endif

namespace Base
{


class BaseExport StdInputStream: public XERCES_CPP_NAMESPACE_QUALIFIER BinInputStream
{
public:
    // clang-format off
    explicit StdInputStream(std::istream& Stream,
                   XERCES_CPP_NAMESPACE_QUALIFIER MemoryManager* const manager =
                   XERCES_CPP_NAMESPACE_QUALIFIER XMLPlatformUtils::fgMemoryManager);
    ~StdInputStream() override;
    // clang-format on

    // -----------------------------------------------------------------------
    //  Implementation of the input stream interface
    // -----------------------------------------------------------------------
    XMLFilePos curPos() const override;
    XMLSize_t readBytes(XMLByte* const toFill, const XMLSize_t maxToRead) override;
    const XMLCh* getContentType() const override
    {
        return nullptr;
    }

    // -----------------------------------------------------------------------
    //  Unimplemented constructors and operators
    // -----------------------------------------------------------------------
    StdInputStream(const StdInputStream&) = delete;
    StdInputStream(StdInputStream&&) = delete;
    StdInputStream& operator=(const StdInputStream&) = delete;
    StdInputStream& operator=(StdInputStream&&) = delete;

private:
    // -----------------------------------------------------------------------
    //  Private data members
    //
    //  fSource
    //      The source file that we represent. The FileHandle type is defined
    //      per platform.
    // -----------------------------------------------------------------------
    std::istream& stream;
    struct TextCodec;
    std::unique_ptr<TextCodec> codec;
};


class BaseExport StdInputSource: public XERCES_CPP_NAMESPACE_QUALIFIER InputSource
{
public:
    StdInputSource(std::istream& Stream,
                   const char* filePath,
                   XERCES_CPP_NAMESPACE_QUALIFIER MemoryManager* const manager =
                       XERCES_CPP_NAMESPACE_QUALIFIER XMLPlatformUtils::fgMemoryManager);
    ~StdInputSource() override;

    XERCES_CPP_NAMESPACE_QUALIFIER BinInputStream* makeStream() const override;

    StdInputSource(const StdInputSource&) = delete;
    StdInputSource(StdInputSource&&) = delete;
    StdInputSource& operator=(const StdInputSource&) = delete;
    StdInputSource& operator=(StdInputSource&&) = delete;

private:
    std::istream& stream;
};

}  // namespace Base

#endif  // BASE_IINPUTSOURCE_H
