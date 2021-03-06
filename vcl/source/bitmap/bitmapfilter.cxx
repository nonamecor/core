/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <vcl/BitmapFilter.hxx>

BitmapFilter::BitmapFilter()
{}

BitmapFilter::~BitmapFilter()
{}

bool BitmapFilter::Filter(BitmapEx &rBmpEx, BitmapFilter &&rFilter)
{
    BitmapEx aTmpBmpEx(rFilter.execute(rBmpEx));

    if (aTmpBmpEx.IsEmpty())
    {
        SAL_WARN("vcl.gdi", "Bitmap filter failed");
        return false;
    }

    rBmpEx = aTmpBmpEx;
    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
