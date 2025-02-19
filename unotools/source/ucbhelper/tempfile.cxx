/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <sal/config.h>

#include <cassert>
#include <utility>

#include <unotools/tempfile.hxx>
#include <rtl/ustring.hxx>
#include <osl/mutex.hxx>
#include <osl/detail/file.h>
#include <osl/file.hxx>
#include <tools/time.hxx>
#include <tools/debug.hxx>
#include <comphelper/DirectoryHelper.hxx>

#ifdef UNX
#include <unistd.h>
#elif defined( _WIN32 )
#include <process.h>
#endif

using namespace osl;

namespace
{
    OUString gTempNameBase_Impl;
}

namespace utl
{

static OUString getParentName( std::u16string_view aFileName )
{
    size_t lastIndex = aFileName.rfind( '/' );
    OUString aParent;

    if (lastIndex != std::u16string_view::npos)
    {
        aParent = aFileName.substr(0, lastIndex);

        if (aParent.endsWith(":") && aParent.getLength() == 6)
            aParent += "/";

        if (aParent.equalsIgnoreAsciiCase("file://"))
            aParent = "file:///";
    }

    return aParent;
}

static bool ensuredir( const OUString& rUnqPath )
{
    OUString aPath;
    if ( rUnqPath.isEmpty() )
        return false;

    // remove trailing slash
    if ( rUnqPath.endsWith("/") )
        aPath = rUnqPath.copy( 0, rUnqPath.getLength() - 1 );
    else
        aPath = rUnqPath;

    // HACK: create directory on a mount point with nobrowse option
    // returns ENOSYS in any case !!
    osl::Directory aDirectory( aPath );
    osl::FileBase::RC nError = aDirectory.open();
    aDirectory.close();
    if( nError == osl::File::E_None )
        return true;

    // try to create the directory
    nError = osl::Directory::create( aPath );
    bool  bSuccess = ( nError == osl::File::E_None || nError == osl::FileBase::E_EXIST );
    if( !bSuccess )
    {
        // perhaps parent(s) don't exist
        OUString aParentDir = getParentName( aPath );
        if ( aParentDir != aPath )
        {
            bSuccess = ensuredir( getParentName( aPath ) );

            // After parent directory structure exists try it one's more
            if ( bSuccess )
            {
                // Parent directory exists, retry creation of directory
                nError = osl::Directory::create( aPath );
                bSuccess =( nError == osl::File::E_None || nError == osl::FileBase::E_EXIST );
            }
        }
    }

    return bSuccess;
}

static OUString ConstructTempDir_Impl( const OUString* pParent, bool bCreateParentDirs )
{
    OUString aName;

    // Ignore pParent on iOS. We don't want to create any temp files
    // in the same directory where the document being edited is.
#ifndef IOS
    if ( pParent && !pParent->isEmpty() )
    {
        // test for valid filename
        OUString aRet;
        if ((osl::FileBase::getSystemPathFromFileURL(*pParent, aRet)
             == osl::FileBase::E_None)
            && (osl::FileBase::getFileURLFromSystemPath(aRet, aRet)
                == osl::FileBase::E_None))
        {
            ::osl::DirectoryItem aItem;
            sal_Int32 i = aRet.getLength();
            if ( aRet[i-1] == '/' )
                i--;

            if ( DirectoryItem::get( aRet.copy(0, i), aItem ) == FileBase::E_None || bCreateParentDirs )
                aName = aRet;
        }
    }
#else
    (void) pParent;
    (void) bCreateParentDirs;
#endif

    if ( aName.isEmpty() )
    {
        if (gTempNameBase_Impl.isEmpty())
        {
            OUString ustrTempDirURL;
            ::osl::FileBase::RC rc = ::osl::File::getTempDirURL(
                ustrTempDirURL );
            if (rc == ::osl::FileBase::E_None)
                gTempNameBase_Impl = ustrTempDirURL;
            ensuredir( aName );
        }
        // if no parent or invalid parent : use default directory
        DBG_ASSERT( !gTempNameBase_Impl.isEmpty(), "No TempDir!" );
        aName = gTempNameBase_Impl;
    }

    // Make sure that directory ends with a separator
    if( !aName.isEmpty() && !aName.endsWith("/") )
        aName += "/";

    return aName;
}

namespace {

class Tokens {
public:
    virtual bool next(OUString *) = 0;

protected:
    virtual ~Tokens() {} // avoid warnings
};

class SequentialTokens: public Tokens {
public:
    explicit SequentialTokens(bool showZero): m_value(0), m_show(showZero) {}

    bool next(OUString * token) override {
        assert(token != nullptr);
        if (m_value == SAL_MAX_UINT32) {
            return false;
        }
        *token = m_show ? OUString::number(m_value) : OUString();
        ++m_value;
        m_show = true;
        return true;
    }

private:
    sal_uInt32 m_value;
    bool m_show;
};

class UniqueTokens: public Tokens {
public:
    UniqueTokens(): m_count(0) {}

    bool next(OUString * token) override {
        assert(token != nullptr);
        // Because of the shared globalValue, no single instance of UniqueTokens
        // is guaranteed to exhaustively test all 36^6 possible values, but stop
        // after that many attempts anyway:
        sal_uInt32 radix = 36;
        sal_uInt32 max = radix * radix * radix * radix * radix * radix;
            // 36^6 == 2'176'782'336 < SAL_MAX_UINT32 == 4'294'967'295
        if (m_count == max) {
            return false;
        }
        sal_uInt32 v;
        {
            osl::MutexGuard g(osl::Mutex::getGlobalMutex());
            globalValue
                = ((globalValue == SAL_MAX_UINT32
                    ? tools::Time::GetSystemTicks() : globalValue + 1)
                   % max);
            v = globalValue;
        }
        *token = OUString::number(v, radix);
        ++m_count;
        return true;
    }

private:
    static sal_uInt32 globalValue;

    sal_uInt32 m_count;
};

}

sal_uInt32 UniqueTokens::globalValue = SAL_MAX_UINT32;

namespace
{
    class TempDirCreatedObserver : public DirectoryCreationObserver
    {
    public:
        virtual void DirectoryCreated(const OUString& aDirectoryUrl) override
        {
            File::setAttributes( aDirectoryUrl, osl_File_Attribute_OwnRead |
                osl_File_Attribute_OwnWrite | osl_File_Attribute_OwnExe );
        };
    };
};

static OUString lcl_createName(
    std::u16string_view rLeadingChars, Tokens & tokens, std::u16string_view pExtension,
    const OUString* pParent, bool bDirectory, bool bKeep, bool bLock,
    bool bCreateParentDirs )
{
    OUString aName = ConstructTempDir_Impl( pParent, bCreateParentDirs );
    if ( bCreateParentDirs )
    {
        size_t nOffset = rLeadingChars.rfind(u"/");
        OUString aDirName;
        if (std::u16string_view::npos != nOffset)
            aDirName = aName + rLeadingChars.substr( 0, nOffset );
        else
            aDirName = aName;
        TempDirCreatedObserver observer;
        FileBase::RC err = Directory::createPath( aDirName, &observer );
        if ( err != FileBase::E_None && err != FileBase::E_EXIST )
            return OUString();
    }
    aName += rLeadingChars;

    OUString token;
    while (tokens.next(&token))
    {
        OUString aTmp( aName + token );
        if ( !pExtension.empty() )
            aTmp += pExtension;
        else
            aTmp += ".tmp";
        if ( bDirectory )
        {
            FileBase::RC err = Directory::create(
                aTmp,
                (osl_File_OpenFlag_Read | osl_File_OpenFlag_Write
                 | osl_File_OpenFlag_Private));
            if ( err == FileBase::E_None )
            {
                // !bKeep: only for creating a name, not a file or directory
                if ( bKeep || Directory::remove( aTmp ) == FileBase::E_None )
                    return aTmp;
                else
                    return OUString();
            }
            else if ( err != FileBase::E_EXIST )
                // if f.e. name contains invalid chars stop trying to create dirs
                return OUString();
        }
        else
        {
            DBG_ASSERT( bKeep, "Too expensive, use directory for creating name!" );
            File aFile( aTmp );
            FileBase::RC err = aFile.open(
                osl_File_OpenFlag_Create | osl_File_OpenFlag_Private
                | (bLock ? 0 : osl_File_OpenFlag_NoLock));
            if ( err == FileBase::E_None || (bLock && err == FileBase::E_NOLCK) )
            {
                aFile.close();
                return aTmp;
            }
            else if ( err != FileBase::E_EXIST )
            {
                // if f.e. name contains invalid chars stop trying to create dirs
                // but if there is a folder with such name proceed further

                DirectoryItem aTmpItem;
                FileStatus aTmpStatus( osl_FileStatus_Mask_Type );
                if ( DirectoryItem::get( aTmp, aTmpItem ) != FileBase::E_None
                  || aTmpItem.getFileStatus( aTmpStatus ) != FileBase::E_None
                  || aTmpStatus.getFileType() != FileStatus::Directory )
                    return OUString();
            }
        }
    }
    return OUString();
}

static OUString CreateTempName_Impl( const OUString* pParent, bool bKeep, bool bDir = true )
{
    OUString aEyeCatcher = "lu";
#ifdef UNX
#ifdef DBG_UTIL
    const char* eye = getenv("LO_TESTNAME");
    if(eye)
    {
        aEyeCatcher = OUString(eye, strlen(eye), RTL_TEXTENCODING_ASCII_US);
    }
#else
    static const pid_t pid = getpid();
    static const OUString aPidString = OUString::number(pid);
    aEyeCatcher += aPidString;
#endif
#elif defined(_WIN32)
    static const int pid = _getpid();
    static const OUString aPidString = OUString::number(pid);
    aEyeCatcher += aPidString;
#endif
    UniqueTokens t;
    return lcl_createName( aEyeCatcher, t, u"", pParent, bDir, bKeep,
                           false, false);
}

OUString CreateTempName()
{
    OUString aName(CreateTempName_Impl( nullptr, false ));

    // convert to file URL
    OUString aTmp;
    if ( !aName.isEmpty() )
        FileBase::getSystemPathFromFileURL( aName, aTmp );
    return aTmp;
}

TempFileFast::TempFileFast( )
{
}

TempFileFast::TempFileFast(TempFileFast && other) noexcept :
    mxStream(std::move(other.mxStream))
{
}

TempFileFast::~TempFileFast()
{
}

SvStream* TempFileFast::GetStream( StreamMode eMode )
{
    if (!mxStream)
    {
        OUString aName = CreateTempName_Impl( /*pParent*/nullptr, /*bKeep*/true, /*bDirectory*/false );
        mxStream.reset(new SvFileStream(aName, eMode | StreamMode::TEMPORARY));
    }
    return mxStream.get();
}

void TempFileFast::CloseStream()
{
    mxStream.reset();
}

OUString CreateTempURL( const OUString* pParent, bool bDirectory )
{
    return CreateTempName_Impl( pParent, true, bDirectory );
}

OUString CreateTempURL( std::u16string_view rLeadingChars, bool _bStartWithZero,
                    std::u16string_view pExtension, const OUString* pParent,
                    bool bCreateParentDirs )
{
    SequentialTokens t(_bStartWithZero);
    return lcl_createName( rLeadingChars, t, pExtension, pParent, false,
                            true, true, bCreateParentDirs );
}

TempFileNamed::TempFileNamed( const OUString* pParent, bool bDirectory )
    : bIsDirectory( bDirectory )
    , bKillingFileEnabled( false )
{
    aName = CreateTempName_Impl( pParent, true, bDirectory );
}

TempFileNamed::TempFileNamed( std::u16string_view rLeadingChars, bool _bStartWithZero,
                    std::u16string_view pExtension, const OUString* pParent,
                    bool bCreateParentDirs )
    : bIsDirectory( false )
    , bKillingFileEnabled( false )
{
    SequentialTokens t(_bStartWithZero);
    aName = lcl_createName( rLeadingChars, t, pExtension, pParent, false,
                            true, true, bCreateParentDirs );
}

TempFileNamed::TempFileNamed(TempFileNamed && other) noexcept :
    aName(std::move(other.aName)), pStream(std::move(other.pStream)), bIsDirectory(other.bIsDirectory),
    bKillingFileEnabled(other.bKillingFileEnabled)
{
    other.bKillingFileEnabled = false;
}

TempFileNamed::~TempFileNamed()
{
    if ( !bKillingFileEnabled )
        return;

    pStream.reset();
    if ( bIsDirectory )
    {
        comphelper::DirectoryHelper::deleteDirRecursively(aName);
    }
    else
    {
        File::remove( aName );
    }
}

bool TempFileNamed::IsValid() const
{
    return !aName.isEmpty();
}

OUString TempFileNamed::GetFileName() const
{
    OUString aTmp;
    FileBase::getSystemPathFromFileURL(aName, aTmp);
    return aTmp;
}

OUString const & TempFileNamed::GetURL() const
{
    // if you request the URL, then you presumably want to access this via UCB,
    // and UCB will want to open the file via a separate file handle, which means
    // we have to make this file data actually hit disk. We do this here (and not
    // elsewhere) to make the other (normal) paths fast. Flushing to disk
    // really slows temp files down.
    if (pStream)
        pStream->Flush();
    return aName;
}

SvStream* TempFileNamed::GetStream( StreamMode eMode )
{
    if (!pStream)
    {
        if (!aName.isEmpty())
            pStream.reset(new SvFileStream(aName, eMode | StreamMode::TEMPORARY));
        else
            pStream.reset(new SvMemoryStream);
    }

    return pStream.get();
}

void TempFileNamed::CloseStream()
{
    pStream.reset();
}

OUString SetTempNameBaseDirectory( const OUString &rBaseName )
{
    if( rBaseName.isEmpty() )
        return OUString();

    OUString aUnqPath( rBaseName );

    // remove trailing slash
    if ( rBaseName.endsWith("/") )
        aUnqPath = rBaseName.copy( 0, rBaseName.getLength() - 1 );

    // try to create the directory
    bool bRet = false;
    osl::FileBase::RC err = osl::Directory::create( aUnqPath );
    if ( err != FileBase::E_None && err != FileBase::E_EXIST )
        // perhaps parent(s) don't exist
        bRet = ensuredir( aUnqPath );
    else
        bRet = true;

    // failure to create base directory means returning an empty string
    OUString aTmp;
    if ( bRet )
    {
        // append own internal directory
        OUString &rTempNameBase_Impl = gTempNameBase_Impl;
        rTempNameBase_Impl = rBaseName + "/";

        TempFileNamed aBase( {}, true );
        if ( aBase.IsValid() )
            // use it in case of success
            rTempNameBase_Impl = aBase.aName;

        // return system path of used directory
        FileBase::getSystemPathFromFileURL( rTempNameBase_Impl, aTmp );
    }

    return aTmp;
}

OUString GetTempNameBaseDirectory()
{
    return ConstructTempDir_Impl(nullptr, false);
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
