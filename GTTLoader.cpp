// Wincrypted texture code & CWinCrypt::DecryptBuffer credits: Tahsin
// CWinCrypt class credits: Twostars
#include "stdafx.h"
#include "GTTLoader.h"
#include <cmath>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <filesystem>
#include <d3dcompiler.h>
#include <QDebug>
#include "CWinCrypt.h"

LoadedTexture GTTLoader::LoadTextureAtOffset(ID3D11Device* device, const QByteArray& data, qint64 offset) {
    LoadedTexture result = { nullptr, 0, 0 };

    if (!device || offset < 0 || offset + sizeof(NTF3Header) > data.size())
    {
        QString message = QString("Invalid input detected.\n"
            "Device: %1\nOffset: %2\nData size: %3\nExpected min size: %4")
            .arg(device ? "valid" : "null").arg(offset).arg(data.size())
            .arg(offset + sizeof(NTF3Header));
#ifdef _DEBUG
        qDebug() << message;
#else
        QMessageBox::critical(nullptr, "Texture Load Error", message);
        DebugLogger::log(message);
#endif
        return result;
    }
    CWinCrypt crypt;
    const char* raw = data.constData();
    const char* ptr = raw + offset;

    NTF3Header header;
    memcpy(&header, ptr, sizeof(NTF3Header));
    ptr += sizeof(NTF3Header);

    unsigned char version = static_cast<unsigned char>(header.szID[3]);
    if (memcmp(header.szID, "NTF", 3) != 0 || (version != 0x03 && version != 0x07)) {
#ifdef _DEBUG
        qDebug() << "Invalid header at offset:" << offset << "version:" << version;
#else
        QString message = QString("Invalid header at offset: %1\nVersion: %2").arg(offset).arg(version);
        QMessageBox::critical(nullptr, "Invalid Texture Header", message);
        DebugLogger::log(message);
#endif
        return result;
    }

    if (header.szID[3] == 7 && !crypt.Load()) {
#ifdef _DEBUG
        qDebug() << "Failed to load WinCrypt at offset:" << offset << "version:" << version;
#else
        QString message = QString("Failed to load WinCrypt at offset: %1\nVersion: %2").arg(offset).arg(version);
        QMessageBox::critical(nullptr, "WinCrypt Error", message);
        DebugLogger::log(message);
#endif
        return result;
    }

    DXGI_FORMAT dxgiFormat = GTTLoader::ConvertToDXGIFormat(header.Format);
    if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
#ifdef _DEBUG
        qDebug() << "Unsupported texture format:" << header.Format;
#else
        QString message = QString("Unsupported texture format: 0x%1").arg(header.Format, 8, 16, QLatin1Char('0'));
        QMessageBox::critical(nullptr, "Unsupported Format", message);
        DebugLogger::log(message);
#endif
        return result;
    }

    uint32_t mipLevels = GTTLoader::CalculateMipLevels(header.nWidth, header.nHeight, header.bMipMap);
    std::vector<std::vector<uint8_t>> mipData(mipLevels);

    for (uint32_t i = 0; i < mipLevels; ++i) {
        uint32_t mipW = std::max(header.nWidth >> i, 1u);
        uint32_t mipH = std::max(header.nHeight >> i, 1u);
        size_t size = (dxgiFormat >= DXGI_FORMAT_BC1_UNORM && dxgiFormat <= DXGI_FORMAT_BC3_UNORM)
            ? ((mipW + 3) / 4) * ((mipH + 3) / 4) * (dxgiFormat == DXGI_FORMAT_BC1_UNORM ? 8 : 16)
            : mipW * mipH * ((header.Format == 21) ? 2 : 4);

        if (ptr + size > raw + data.size()) return result;

        mipData[i].resize(size);
        memcpy(mipData[i].data(), ptr, size);
        ptr += size;

        if (header.szID[3] == 7) {
            if (!crypt.DecryptBuffer(mipData[i].data(), size)) {
#ifdef _DEBUG
                qDebug() << "Decryption failed at mip level" << i << "Offset:" << offset;
#else
                QString message = QString("Decryption failed at mip level: %1\nOffset: %2").arg(i).arg(offset);
                QMessageBox::critical(nullptr, "Decryption Failed", message);
                DebugLogger::log(message);
#endif
                return result;
            }
        }

        if (header.Format == 22)
            mipData[i] = GTTLoader::ConvertA4R4G4B4ToR8G8B8A8(mipData[i].data(), mipW, mipH);
    }
    // So if we actually read these instead, uncompressed fallback the image which would have been sharper lol..
    // We don't care anyway we are only writing a viewer not editor, I will just apply anisotropic filtering to the rendered images instead not the best option but who cares.
    // It's not like MGAME are saints....
    if (header.Format >= 0x31545844 && header.Format <= 0x35545844) { 
        uint32_t skipSize = 0;
        uint32_t iWTmp = header.nWidth;
        uint32_t iHTmp = header.nHeight;

        if (header.bMipMap) {
            while (iWTmp >= 4 && iHTmp >= 4) {
                skipSize += (header.Format == 0x31545844) ?
                    iWTmp * iHTmp / 2 : iWTmp * iHTmp;
                iWTmp /= 2;
                iHTmp /= 2;
            }

            iWTmp = header.nWidth / 2;
            iHTmp = header.nHeight / 2;
            while (iWTmp >= 4 && iHTmp >= 4) {
                skipSize += iWTmp * iHTmp * 2;
                iWTmp /= 2;
                iHTmp /= 2;
            }
        }
        else {
            skipSize += (header.Format == 0x31545844) ?
                header.nWidth * header.nHeight / 2 :
                header.nWidth * header.nHeight;
            skipSize += header.nWidth * header.nHeight * 2;
            if (header.nWidth >= 1024)
                skipSize += 256 * 256 * 2;
        }

        ptr += skipSize;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = header.nWidth;
    desc.Height = header.nHeight;
    desc.MipLevels = 1; // We are clamping mip lod to 0 because some have invalid data and we don't care about as this is not a editor but viewer and parser instead...
    desc.ArraySize = 1;
    desc.Format = dxgiFormat;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    std::vector<D3D11_SUBRESOURCE_DATA> subs(mipLevels);
    for (uint32_t i = 0; i < mipLevels; ++i) {
        uint32_t mipW = std::max(header.nWidth >> i, 1u);
        size_t bpp = (header.Format == 21) ? 2 : 4;
        subs[i].pSysMem = mipData[i].data();
        subs[i].SysMemPitch = (dxgiFormat >= DXGI_FORMAT_BC1_UNORM && dxgiFormat <= DXGI_FORMAT_BC3_UNORM)
            ? ((mipW + 3) / 4) * ((dxgiFormat == DXGI_FORMAT_BC1_UNORM) ? 8 : 16)
            : mipW * bpp;
    }

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, subs.data(), &tex)))
    {
        return result;
    }

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(device->CreateShaderResourceView(tex, nullptr, &srv))) 
    {
        tex->Release(); return result;
    }

    result.srv = srv;
    result.width = header.nWidth;
    result.height = header.nHeight;
    tex->Release();

    return result;
}

DXGI_FORMAT GTTLoader::ConvertToDXGIFormat(uint32_t format) {
    switch (format) {
        case 0x31545844: return DXGI_FORMAT_BC1_UNORM;
        case 0x33545844: return DXGI_FORMAT_BC2_UNORM;
        case 0x35545844: return DXGI_FORMAT_BC3_UNORM;
        case 21: return DXGI_FORMAT_B5G5R5A1_UNORM;
        case 22: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case 23: return DXGI_FORMAT_B8G8R8X8_UNORM;
        case 20: return DXGI_FORMAT_R8G8B8A8_UNORM;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

std::vector<uint8_t> GTTLoader::ConvertA4R4G4B4ToR8G8B8A8(const uint8_t* src, uint32_t width, uint32_t height) {
    std::vector<uint8_t> dst(width * height * 4);
    const uint16_t* pixels = reinterpret_cast<const uint16_t*>(src);
    for (uint32_t i = 0; i < width * height; i++) {
        uint16_t pixel = pixels[i];
        dst[i * 4 + 0] = static_cast<uint8_t>((pixel & 0x0F00) >> 4);  // R
        dst[i * 4 + 1] = static_cast<uint8_t>((pixel & 0x00F0));       // G
        dst[i * 4 + 2] = static_cast<uint8_t>((pixel & 0x000F) << 4);  // B
        dst[i * 4 + 3] = static_cast<uint8_t>((pixel & 0xF000) >> 8);  // A
    }
    return dst;
}

uint32_t GTTLoader::CalculateMipLevels(uint32_t width, uint32_t height, bool hasMips) {
    if (!hasMips) return 1;
    return static_cast<uint32_t>(std::log2(std::max(width, height))) + 1;
}