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

#include <basegfx/range/b2drange.hxx>
#include <basegfx/range/b2drectangle.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <sal/log.hxx>
#include "emfpbrush.hxx"
#include "emfppath.hxx"

namespace emfplushelper
{
    EMFPBrush::EMFPBrush()
        : type(0)
        , additionalFlags(0)
        , wrapMode(0)
        , firstPointX(0.0)
        , firstPointY(0.0)
        , secondPointX(0.0)
        , secondPointY(0.0)
        , hasTransformation(false)
        , blendPoints(0)
        , blendFactors(nullptr)
        , colorblendPoints(0)
        , surroundColorsNumber(0)
        , hatchStyle(HatchStyleHorizontal)
    {
    }

    EMFPBrush::~EMFPBrush()
    {
    }

    void EMFPBrush::Read(SvStream& s, EmfPlusHelperData const & rR)
    {
        sal_uInt32 header;

        s.ReadUInt32(header).ReadUInt32(type);

        SAL_INFO("drawinglayer", "EMF+\tbrush");
        SAL_INFO("drawinglayer", "EMF+\theader: 0x" << std::hex << header << " type: " << type << std::dec);

        switch (type)
        {
            case BrushTypeSolidColor:
            {
                sal_uInt32 color;
                s.ReadUInt32(color);

                solidColor = ::Color(0xff - (color >> 24), (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
                SAL_INFO("drawinglayer", "EMF+\tsolid color: 0x" << std::hex << color << std::dec);
                break;
            }
            case BrushTypeHatchFill:
            {
                sal_uInt32 style;
                sal_uInt32 foregroundColor;
                sal_uInt32 backgroundColor;
                s.ReadUInt32(style);
                s.ReadUInt32(foregroundColor);
                s.ReadUInt32(backgroundColor);

                hatchStyle = static_cast<EmfPlusHatchStyle>(style);
                solidColor = ::Color(0xff - (foregroundColor >> 24), (foregroundColor >> 16) & 0xff, (foregroundColor >> 8) & 0xff, foregroundColor & 0xff);
                secondColor = ::Color(0xff - (backgroundColor >> 24), (backgroundColor >> 16) & 0xff, (backgroundColor >> 8) & 0xff, backgroundColor & 0xff);
                SAL_INFO("drawinglayer", "EMF+\thatch style " << style << " foregroundcolor: 0x" << solidColor.AsRGBHexString() << " background 0x" << secondColor.AsRGBHexString());
                break;
            }
            case BrushTypeTextureFill:
            {
                SAL_WARN("drawinglayer", "EMF+\tTODO: implement BrushTypeTextureFill brush");
                break;
            }
            case BrushTypePathGradient:
            {
                s.ReadUInt32(additionalFlags).ReadInt32(wrapMode);
                SAL_INFO("drawinglayer", "EMF+\tpath gradient, additional flags: 0x" << std::hex << additionalFlags << std::dec);
                sal_uInt32 color;
                s.ReadUInt32(color);
                solidColor = ::Color(0xff - (color >> 24), (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
                SAL_INFO("drawinglayer", "EMF+\tcenter color: 0x" << std::hex << color << std::dec);
                s.ReadFloat(firstPointX).ReadFloat(firstPointY);
                SAL_INFO("drawinglayer", "EMF+\tcenter point: " << firstPointX << "," << firstPointY);
                s.ReadInt32(surroundColorsNumber);
                SAL_INFO("drawinglayer", "EMF+\t number of surround colors: " << surroundColorsNumber);

                if (surroundColorsNumber<0 || sal_uInt32(surroundColorsNumber)>SAL_MAX_INT32 / sizeof(::Color))
                {
                    surroundColorsNumber = SAL_MAX_INT32 / sizeof(::Color);
                }

                surroundColors.reset( new ::Color[surroundColorsNumber] );

                for (int i = 0; i < surroundColorsNumber; i++)
                {
                    s.ReadUInt32(color);
                    surroundColors[i] = ::Color(0xff - (color >> 24), (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
                    if (i == 0)
                        secondColor = surroundColors[0];
                    SAL_INFO("drawinglayer", "EMF+\tsurround color[" << i << "]: 0x" << std::hex << color << std::dec);
                }

                if (additionalFlags & 0x01)
                {
                    sal_Int32 pathLength;

                    s.ReadInt32(pathLength);
                    SAL_INFO("drawinglayer", "EMF+\tpath length: " << pathLength);

                    sal_uInt64 const pos = s.Tell();

                    sal_uInt32 pathHeader;
                    sal_Int32 pathPoints, pathFlags;
                    s.ReadUInt32(pathHeader).ReadInt32(pathPoints).ReadInt32(pathFlags);

                    SAL_INFO("drawinglayer", "EMF+\tpath (brush path gradient)");
                    SAL_INFO("drawinglayer", "EMF+\theader: 0x" << std::hex << pathHeader << " points: " << std::dec << pathPoints << " additional flags: 0x" << std::hex << pathFlags << std::dec);

                    path.reset( new EMFPPath(pathPoints) );
                    path->Read(s, pathFlags);

                    s.Seek(pos + pathLength);

                    const ::basegfx::B2DRectangle aBounds(::basegfx::utils::getRange(path->GetPolygon(rR, false)));
                    secondPointX = aBounds.getWidth();
                    secondPointY = aBounds.getHeight();
                    SAL_INFO("drawinglayer", "EMF+\t polygon bounding box: " << aBounds.getMinX() << "," << aBounds.getMinY() << " " << aBounds.getWidth() << "x" << aBounds.getHeight());
                }
                else
                {
                    sal_Int32 boundaryPointCount;
                    s.ReadInt32(boundaryPointCount);

                    sal_uInt64 const pos = s.Tell();
                    SAL_INFO("drawinglayer", "EMF+\t use boundary, points: " << boundaryPointCount);
                    path.reset( new EMFPPath(boundaryPointCount) );
                    path->Read(s, 0x0);

                    s.Seek(pos + 8 * boundaryPointCount);

                    const ::basegfx::B2DRectangle aBounds(::basegfx::utils::getRange(path->GetPolygon(rR, false)));
                    secondPointX = aBounds.getWidth();
                    secondPointY = aBounds.getHeight();
                    SAL_INFO("drawinglayer", "EMF+\t polygon bounding box: " << aBounds.getMinX() << "," << aBounds.getMinY() << " " << aBounds.getWidth() << "x" << aBounds.getHeight());
                }

                if (additionalFlags & 0x02)
                {
                    EmfPlusHelperData::readXForm(s, brush_transformation);
                    hasTransformation = true;
                    SAL_INFO("drawinglayer", "EMF+\tuse brush transformation: " << brush_transformation);
                }

                if (additionalFlags & 0x08)
                {
                    s.ReadInt32(blendPoints);
                    SAL_INFO("drawinglayer", "EMF+\tuse blend, points: " << blendPoints);
                    if (blendPoints<0 || sal_uInt32(blendPoints)>SAL_MAX_INT32 / (2 * sizeof(float)))
                        blendPoints = SAL_MAX_INT32 / (2 * sizeof(float));
                    blendPositions.reset( new float[2 * blendPoints] );
                    blendFactors = blendPositions.get() + blendPoints;

                    for (int i = 0; i < blendPoints; i++)
                    {
                        s.ReadFloat(blendPositions[i]);
                        SAL_INFO("drawinglayer", "EMF+\tposition[" << i << "]: " << blendPositions[i]);
                    }

                    for (int i = 0; i < blendPoints; i++)
                    {
                        s.ReadFloat(blendFactors[i]);
                        SAL_INFO("drawinglayer", "EMF+\tfactor[" << i << "]: " << blendFactors[i]);
                    }
                }

                if (additionalFlags & 0x04)
                {
                    s.ReadInt32(colorblendPoints);
                    SAL_INFO("drawinglayer", "EMF+\tuse color blend, points: " << colorblendPoints);

                    if (colorblendPoints<0 || sal_uInt32(colorblendPoints)>SAL_MAX_INT32 / sizeof(float))
                    {
                        colorblendPoints = SAL_MAX_INT32 / sizeof(float);
                    }

                    if (sal_uInt32(colorblendPoints) > SAL_MAX_INT32 / sizeof(::Color))
                    {
                        colorblendPoints = SAL_MAX_INT32 / sizeof(::Color);
                    }

                    colorblendPositions.reset( new float[colorblendPoints] );
                    colorblendColors.reset( new ::Color[colorblendPoints] );

                    for (int i = 0; i < colorblendPoints; i++)
                    {
                        s.ReadFloat(colorblendPositions[i]);
                        SAL_INFO("drawinglayer", "EMF+\tposition[" << i << "]: " << colorblendPositions[i]);
                    }

                    for (int i = 0; i < colorblendPoints; i++)
                    {
                        s.ReadUInt32(color);
                        colorblendColors[i] = ::Color(0xff - (color >> 24), (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
                        SAL_INFO("drawinglayer", "EMF+\tcolor[" << i << "]: 0x" << std::hex << color << std::dec);
                    }
                }

                break;
            }
            case BrushTypeLinearGradient:
            {
                s.ReadUInt32(additionalFlags).ReadInt32(wrapMode);
                SAL_INFO("drawinglayer", "EMF+\tlinear gradient, additional flags: 0x" << std::hex << additionalFlags << std::dec);
                s.ReadFloat(firstPointX).ReadFloat(firstPointY).ReadFloat(secondPointX).ReadFloat(secondPointY);
                SAL_INFO("drawinglayer", "EMF+\tFirst gradinet point: " << firstPointX << ":" << firstPointY << ", second gradient point " << secondPointX << ":" << secondPointY);
                sal_uInt32 color;
                s.ReadUInt32(color);
                solidColor = ::Color(0xff - (color >> 24), (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
                SAL_INFO("drawinglayer", "EMF+\tfirst color: 0x" << std::hex << color << std::dec);
                s.ReadUInt32(color);
                secondColor = ::Color(0xff - (color >> 24), (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
                SAL_INFO("drawinglayer", "EMF+\tsecond color: 0x" << std::hex << color << std::dec);

                // repeated colors, unknown meaning, see http://www.aces.uiuc.edu/~jhtodd/Metafile/MetafileRecords/ObjectBrush.html
                s.ReadUInt32(color);
                s.ReadUInt32(color);

                if (additionalFlags & 0x02)
                {
                    EmfPlusHelperData::readXForm(s, brush_transformation);
                    hasTransformation = true;
                    SAL_INFO("drawinglayer", "EMF+\tuse brush transformation: " << brush_transformation);
                }

                if (additionalFlags & 0x08)
                {
                    s.ReadInt32(blendPoints);
                    SAL_INFO("drawinglayer", "EMF+\tuse blend, points: " << blendPoints);
                    if (blendPoints<0 || sal_uInt32(blendPoints)>SAL_MAX_INT32 / (2 * sizeof(float)))
                        blendPoints = SAL_MAX_INT32 / (2 * sizeof(float));
                    blendPositions.reset( new float[2 * blendPoints] );
                    blendFactors = blendPositions.get() + blendPoints;

                    for (int i = 0; i < blendPoints; i++)
                    {
                        s.ReadFloat(blendPositions[i]);
                        SAL_INFO("drawinglayer", "EMF+\tposition[" << i << "]: " << blendPositions[i]);
                    }

                    for (int i = 0; i < blendPoints; i++)
                    {
                        s.ReadFloat(blendFactors[i]);
                        SAL_INFO("drawinglayer", "EMF+\tfactor[" << i << "]: " << blendFactors[i]);
                    }
                }

                if (additionalFlags & 0x04)
                {
                    s.ReadInt32(colorblendPoints);
                    SAL_INFO("drawinglayer", "EMF+\tuse color blend, points: " << colorblendPoints);

                    if (colorblendPoints<0 || sal_uInt32(colorblendPoints)>SAL_MAX_INT32 / sizeof(float))
                    {
                        colorblendPoints = SAL_MAX_INT32 / sizeof(float);
                    }

                    if (sal_uInt32(colorblendPoints) > SAL_MAX_INT32 / sizeof(::Color))
                    {
                        colorblendPoints = sal_uInt32(SAL_MAX_INT32) / sizeof(::Color);
                    }

                    colorblendPositions.reset( new float[colorblendPoints] );
                    colorblendColors.reset( new ::Color[colorblendPoints] );

                    for (int i = 0; i < colorblendPoints; i++)
                    {
                        s.ReadFloat(colorblendPositions[i]);
                        SAL_INFO("drawinglayer", "EMF+\tposition[" << i << "]: " << colorblendPositions[i]);
                    }

                    for (int i = 0; i < colorblendPoints; i++)
                    {
                        s.ReadUInt32(color);
                        colorblendColors[i] = ::Color(0xff - (color >> 24), (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
                        SAL_INFO("drawinglayer", "EMF+\tcolor[" << i << "]: 0x" << std::hex << color << std::dec);
                    }
                }

                break;
            }
            default:
            {
                SAL_INFO("drawinglayer", "EMF+\tunhandled brush type: " << std::hex << type << std::dec);
            }
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
