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

#include "dbfindex.hxx"
#include <comphelper/processfactory.hxx>
#include <osl/file.hxx>
#include <tools/config.hxx>
#include <sfx2/app.hxx>
#include <dbu_dlg.hxx>
#include <osl/diagnose.h>
#include <unotools/localfilehelper.hxx>
#include <tools/urlobj.hxx>
#include <unotools/pathoptions.hxx>
#include <ucbhelper/content.hxx>
#include <svl/filenotation.hxx>
#include <rtl/strbuf.hxx>

namespace dbaui
{
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::ucb;
using namespace ::svt;

const OString aGroupIdent("dBase III");


ODbaseIndexDialog::ODbaseIndexDialog(vcl::Window * pParent, const OUString& aDataSrcName)
    : ModalDialog(pParent, "DBaseIndexDialog", "dbaccess/ui/dbaseindexdialog.ui")
    , m_aDSN(aDataSrcName)
{
    get(m_pPB_OK, "ok");
    get(m_pCB_Tables, "table");
    get(m_pIndexes, "frame");
    get(m_pLB_TableIndexes, "tableindex");
    get(m_pLB_FreeIndexes, "freeindex");
    Size aSize(LogicToPixel(Size(76, 98), MapMode(MapUnit::MapAppFont)));
    m_pLB_TableIndexes->set_height_request(aSize.Height());
    m_pLB_TableIndexes->set_width_request(aSize.Width());
    m_pLB_FreeIndexes->set_height_request(aSize.Height());
    m_pLB_FreeIndexes->set_width_request(aSize.Width());
    get(m_pAdd, "add");
    get(m_pAddAll, "addall");
    get(m_pRemove, "remove");
    get(m_pRemoveAll, "removeall");


    m_pCB_Tables->SetSelectHdl( LINK(this, ODbaseIndexDialog, TableSelectHdl) );
    m_pAdd->SetClickHdl( LINK(this, ODbaseIndexDialog, AddClickHdl) );
    m_pRemove->SetClickHdl( LINK(this, ODbaseIndexDialog, RemoveClickHdl) );
    m_pAddAll->SetClickHdl( LINK(this, ODbaseIndexDialog, AddAllClickHdl) );
    m_pRemoveAll->SetClickHdl( LINK(this, ODbaseIndexDialog, RemoveAllClickHdl) );
    m_pPB_OK->SetClickHdl( LINK(this, ODbaseIndexDialog, OKClickHdl) );

    m_pLB_FreeIndexes->SetSelectHdl( LINK(this, ODbaseIndexDialog, OnListEntrySelected) );
    m_pLB_TableIndexes->SetSelectHdl( LINK(this, ODbaseIndexDialog, OnListEntrySelected) );

    m_pCB_Tables->SetDropDownLineCount(8);
    Init();
    SetCtrls();
}

ODbaseIndexDialog::~ODbaseIndexDialog()
{
    disposeOnce();
}

void ODbaseIndexDialog::dispose()
{
    m_pPB_OK.clear();
    m_pCB_Tables.clear();
    m_pIndexes.clear();
    m_pLB_TableIndexes.clear();
    m_pLB_FreeIndexes.clear();
    m_pAdd.clear();
    m_pRemove.clear();
    m_pAddAll.clear();
    m_pRemoveAll.clear();
    ModalDialog::dispose();
}

void ODbaseIndexDialog::checkButtons()
{
    m_pAdd->Enable(0 != m_pLB_FreeIndexes->GetSelectedEntryCount());
    m_pAddAll->Enable(0 != m_pLB_FreeIndexes->GetEntryCount());

    m_pRemove->Enable(0 != m_pLB_TableIndexes->GetSelectedEntryCount());
    m_pRemoveAll->Enable(0 != m_pLB_TableIndexes->GetEntryCount());
}

OTableIndex ODbaseIndexDialog::implRemoveIndex(const OUString& _rName, TableIndexList& _rList, ListBox& _rDisplay, bool _bMustExist)
{
    OTableIndex aReturn;

    sal_Int32 nPos = 0;

    TableIndexList::iterator aSearch;
    for (   aSearch = _rList.begin();
            aSearch != _rList.end();
            ++aSearch, ++nPos
        )
    {
        if ( aSearch->GetIndexFileName() == _rName )
        {
            aReturn = *aSearch;

            _rList.erase(aSearch);
            _rDisplay.RemoveEntry( _rName );

            // adjust selection if necessary
            if (static_cast<sal_uInt32>(nPos) == _rList.size())
                _rDisplay.SelectEntryPos(static_cast<sal_uInt16>(nPos)-1);
            else
                _rDisplay.SelectEntryPos(static_cast<sal_uInt16>(nPos));

            break;
        }
    }

    OSL_ENSURE(!_bMustExist || (aSearch != _rList.end()), "ODbaseIndexDialog::implRemoveIndex : did not find the index!");
    return aReturn;
}

void ODbaseIndexDialog::implInsertIndex(const OTableIndex& _rIndex, TableIndexList& _rList, ListBox& _rDisplay)
{
    _rList.push_front( _rIndex );
    _rDisplay.InsertEntry( _rIndex.GetIndexFileName() );
    _rDisplay.SelectEntryPos(0);
}

OTableIndex ODbaseIndexDialog::RemoveTableIndex( const OUString& _rTableName, const OUString& _rIndexName )
{
    OTableIndex aReturn;

    // does the table exist ?
    TableInfoList::iterator aTablePos = std::find_if(m_aTableInfoList.begin(), m_aTableInfoList.end(),
                                           [&] (const OTableInfo& arg) { return arg.aTableName == _rTableName; });

    if (aTablePos == m_aTableInfoList.end())
        return aReturn;

    return implRemoveIndex(_rIndexName, aTablePos->aIndexList, *m_pLB_TableIndexes, true/*_bMustExist*/);
}

void ODbaseIndexDialog::InsertTableIndex( const OUString& _rTableName, const OTableIndex& _rIndex)
{
    TableInfoList::iterator aTablePos = std::find_if(m_aTableInfoList.begin(), m_aTableInfoList.end(),
                                           [&] (const OTableInfo& arg) { return arg.aTableName == _rTableName; });

    if (aTablePos == m_aTableInfoList.end())
        return;

    implInsertIndex(_rIndex, aTablePos->aIndexList, *m_pLB_TableIndexes);
}

IMPL_LINK_NOARG( ODbaseIndexDialog, OKClickHdl, Button*, void )
{
    // let all tables write their INF file

    for (auto const& tableInfo : m_aTableInfoList)
        tableInfo.WriteInfFile(m_aDSN);

    EndDialog();
}

IMPL_LINK_NOARG( ODbaseIndexDialog, AddClickHdl, Button*, void )
{
    OUString aSelection = m_pLB_FreeIndexes->GetSelectedEntry();
    OUString aTableName = m_pCB_Tables->GetText();
    OTableIndex aIndex = RemoveFreeIndex( aSelection, true );
    InsertTableIndex( aTableName, aIndex );

    checkButtons();
}

IMPL_LINK_NOARG( ODbaseIndexDialog, RemoveClickHdl, Button*, void )
{
    OUString aSelection = m_pLB_TableIndexes->GetSelectedEntry();
    OUString aTableName = m_pCB_Tables->GetText();
    OTableIndex aIndex = RemoveTableIndex( aTableName, aSelection );
    InsertFreeIndex( aIndex );

    checkButtons();
}

IMPL_LINK_NOARG( ODbaseIndexDialog, AddAllClickHdl, Button*, void )
{
    const sal_Int32 nCnt = m_pLB_FreeIndexes->GetEntryCount();
    OUString aTableName = m_pCB_Tables->GetText();

    for( sal_Int32 nPos = 0; nPos < nCnt; ++nPos )
        InsertTableIndex( aTableName, RemoveFreeIndex( m_pLB_FreeIndexes->GetEntry(0), true ) );

    checkButtons();
}

IMPL_LINK_NOARG( ODbaseIndexDialog, RemoveAllClickHdl, Button*, void )
{
    const sal_Int32 nCnt = m_pLB_TableIndexes->GetEntryCount();
    OUString aTableName = m_pCB_Tables->GetText();

    for( sal_Int32 nPos = 0; nPos < nCnt; ++nPos )
        InsertFreeIndex( RemoveTableIndex( aTableName, m_pLB_TableIndexes->GetEntry(0) ) );

    checkButtons();
}

IMPL_LINK_NOARG( ODbaseIndexDialog, OnListEntrySelected, ListBox&, void )
{
    checkButtons();
}

IMPL_LINK( ODbaseIndexDialog, TableSelectHdl, ComboBox&, rComboBox, void )
{
    // search the table
    TableInfoList::iterator aTablePos = std::find_if(m_aTableInfoList.begin(), m_aTableInfoList.end(),
                                           [&] (const OTableInfo& arg) { return arg.aTableName == rComboBox.GetText() ; });

    if (aTablePos == m_aTableInfoList.end())
        return;

    // fill the listbox for the indexes
    m_pLB_TableIndexes->Clear();
    for (auto const& index : aTablePos->aIndexList)
        m_pLB_TableIndexes->InsertEntry( index.GetIndexFileName() );

    if ( !aTablePos->aIndexList.empty() )
        m_pLB_TableIndexes->SelectEntryPos(0);

    checkButtons();
}

void ODbaseIndexDialog::Init()
{
    m_pPB_OK->Disable();
    m_pIndexes->Disable();

    // All indices are first added to a list of free indices.
    // Afterwards, check the index of each table in the Inf-file.
    // These indices are removed from the list of free indices and
    // entered in the indexlist of the table.

    // if the string does not contain a path, cut the string
    INetURLObject aURL;
    aURL.SetSmartProtocol(INetProtocol::File);
    {
        SvtPathOptions aPathOptions;
        m_aDSN = aPathOptions.SubstituteVariable(m_aDSN);
    }
    aURL.SetSmartURL(m_aDSN);

    //  String aFileName = aURL.PathToFileName();
    m_aDSN = aURL.GetMainURL(INetURLObject::DecodeMechanism::NONE);
    ::ucbhelper::Content aFile;
    bool bFolder=true;
    try
    {
        aFile = ::ucbhelper::Content(m_aDSN,Reference< css::ucb::XCommandEnvironment >(), comphelper::getProcessComponentContext());
        bFolder = aFile.isFolder();
    }
    catch(Exception&)
    {
        return;
    }

    // first assume for all indexes they're free

    OUString const aIndexExt("ndx");
    OUString const aTableExt("dbf");

    std::vector< OUString > aUsedIndexes;

    aURL.SetSmartProtocol(INetProtocol::File);
    for(const OUString& rURL : ::utl::LocalFileHelper::GetFolderContents(m_aDSN, bFolder))
    {
        OUString aName;
        osl::FileBase::getSystemPathFromFileURL(rURL,aName);
        aURL.SetSmartURL(aName);
        OUString aExt = aURL.getExtension();
        if (aExt == aIndexExt)
        {
            m_aFreeIndexList.emplace_back(aURL.getName() );
        }
        else if (aExt == aTableExt)
        {
            m_aTableInfoList.emplace_back(aURL.getName() );
            OTableInfo& rTabInfo = m_aTableInfoList.back();

            // open the INF file
            aURL.setExtension("inf");
            OFileNotation aTransformer(aURL.GetURLNoPass(), OFileNotation::N_URL);
            Config aInfFile( aTransformer.get(OFileNotation::N_SYSTEM) );
            aInfFile.SetGroup( aGroupIdent );

            // fill the indexes list
            OString aNDX;
            sal_uInt16 nKeyCnt = aInfFile.GetKeyCount();
            OString aKeyName;
            OUString aEntry;

            for( sal_uInt16 nKey = 0; nKey < nKeyCnt; nKey++ )
            {
                // does the key point to an index file ?
                aKeyName = aInfFile.GetKeyName( nKey );
                aNDX = aKeyName.copy(0,3);

                // yes -> add to the tables index list
                if (aNDX == "NDX")
                {
                    aEntry = OStringToOUString(aInfFile.ReadKey(aKeyName), osl_getThreadTextEncoding());
                    rTabInfo.aIndexList.emplace_back( aEntry );

                    // and remove it from the free index list
                    aUsedIndexes.push_back(aEntry);
                        // do this later below. We may not have encountered the index file, yet, thus we may not
                        // know the index as being free, yet
                }
            }
        }
    }

    for (auto const& usedIndex : aUsedIndexes)
        RemoveFreeIndex( usedIndex, false );

    if (!m_aTableInfoList.empty())
    {
        m_pPB_OK->Enable();
        m_pIndexes->Enable();
    }

    checkButtons();
}

void ODbaseIndexDialog::SetCtrls()
{
    // ComboBox tables
    for (auto const& tableInfo : m_aTableInfoList)
        m_pCB_Tables->InsertEntry( tableInfo.aTableName );

    // put the first dataset into Edit
    if( !m_aTableInfoList.empty() )
    {
        const OTableInfo& rTabInfo = m_aTableInfoList.front();
        m_pCB_Tables->SetText( rTabInfo.aTableName );

        // build ListBox of the table indices
        for (auto const& index : rTabInfo.aIndexList)
            m_pLB_TableIndexes->InsertEntry( index.GetIndexFileName() );

        if( !rTabInfo.aIndexList.empty() )
            m_pLB_TableIndexes->SelectEntryPos( 0 );
    }

    // ListBox of the free indices
    for (auto const& freeIndex : m_aFreeIndexList)
        m_pLB_FreeIndexes->InsertEntry( freeIndex.GetIndexFileName() );

    if( !m_aFreeIndexList.empty() )
        m_pLB_FreeIndexes->SelectEntryPos( 0 );

    TableSelectHdl(*m_pCB_Tables);
    checkButtons();
}

void OTableInfo::WriteInfFile( const OUString& rDSN ) const
{
    // open INF file
    INetURLObject aURL;
    aURL.SetSmartProtocol(INetProtocol::File);
    OUString aDsn = rDSN;
    {
        SvtPathOptions aPathOptions;
        aDsn = aPathOptions.SubstituteVariable(aDsn);
    }
    aURL.SetSmartURL(aDsn);
    aURL.Append(aTableName);
    aURL.setExtension("inf");

    OFileNotation aTransformer(aURL.GetURLNoPass(), OFileNotation::N_URL);
    Config aInfFile( aTransformer.get(OFileNotation::N_SYSTEM) );
    aInfFile.SetGroup( aGroupIdent );

    // first, delete all table indices
    OString aNDX;
    sal_uInt16 nKeyCnt = aInfFile.GetKeyCount();
    sal_uInt16 nKey = 0;

    while( nKey < nKeyCnt )
    {
        // Does the key point to an index file?...
        OString aKeyName = aInfFile.GetKeyName( nKey );
        aNDX = aKeyName.copy(0,3);

        //...if yes, delete index file, nKey is at subsequent key
        if (aNDX == "NDX")
        {
            aInfFile.DeleteKey(aKeyName);
            nKeyCnt--;
        }
        else
            nKey++;

    }

    // now add all saved indices
    sal_uInt16 nPos = 0;
    for (auto const& index : aIndexList)
    {
        OStringBuffer aKeyName("NDX");
        if( nPos > 0 )  // first index contains no number
            aKeyName.append(static_cast<sal_Int32>(nPos));
        aInfFile.WriteKey(
            aKeyName.makeStringAndClear(),
            OUStringToOString(index.GetIndexFileName(),
                osl_getThreadTextEncoding()));
        ++nPos;
    }

    aInfFile.Flush();

    // if only [dbase] is left in INF-file, delete file
    if(!nPos)
    {
        try
        {
            ::ucbhelper::Content aContent(aURL.GetURLNoPass(),Reference<XCommandEnvironment>(), comphelper::getProcessComponentContext());
            aContent.executeCommand( "delete", makeAny( true ) );
        }
        catch (const Exception& )
        {
            // simply silent this. The strange algorithm here does a lot of
            // things even if no files at all were created or accessed, so it's
            // possible that the file we're trying to delete does not even
            // exist, and this is a valid condition.
        }
    }
}

} // namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
