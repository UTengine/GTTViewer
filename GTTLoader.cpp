// Wincrypted texture code & CWinCrypt::DecryptBuffer credits: Tahsin
// CWinCrypt class credits: Twostars
#include <algorithm>
#include "stdafx.h"
#include "GTTLoader.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

LoadedTexture GTTLoader::LoadTextureAtOffset(ID3D11Device* device, const QByteArray& data, qint64 offset, const QString& baseName, uint32_t index)
{
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

    const char* ptrBackup = ptr;
    uint32_t validMipLevels = 0;
    size_t totalSize = 0;
    int mipLevels = 0;

        switch (header.Format)
        {
        case 827611204:
        {
            ptr = ptrBackup;
            size_t totalSize = 0;
            uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max<uint32_t>(header.nWidth, header.nHeight)))) + 1;
            uint32_t validMipLevels = 0;
            std::vector<std::vector<uint8_t>> mipData;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max<uint32_t>(header.nWidth >> i, 1u);
                uint32_t mipH = std::max<uint32_t>(header.nHeight >> i, 1u);
                size_t size = ((mipW + 3) / 4) * ((mipH + 3) / 4) * 8;

                if (totalSize + size > static_cast<size_t>(data.size() - (ptr - raw)))
                    break;

                std::vector<uint8_t> mip(size);
                memcpy(mip.data(), ptr + totalSize, size);
                mipData.push_back(std::move(mip));
                totalSize += size;
                ++validMipLevels;
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
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* block = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            auto decodeColor = [](uint16_t c) -> std::array<uint8_t, 3> {
                return {
                    static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31),
                    static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63),
                    static_cast<uint8_t>((c & 0x1F) * 255 / 31)
                };
                };

            for (int y = 0; y < height; y += 4) {
                for (int x = 0; x < width; x += 4, block += 8) {
                    uint16_t c0 = block[0] | (block[1] << 8);
                    uint16_t c1 = block[2] | (block[3] << 8);
                    auto col0 = decodeColor(c0);
                    auto col1 = decodeColor(c1);

                    std::array<std::array<uint8_t, 4>, 4> colors = { {
                        { col0[0], col0[1], col0[2], 255 },
                        { col1[0], col1[1], col1[2], 255 },
                        {}, {}
                    } };

                    if (c0 > c1) {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (2 * col0[i] + col1[i]) / 3;
                        for (int i = 0; i < 3; ++i)
                            colors[3][i] = (col0[i] + 2 * col1[i]) / 3;
                        colors[2][3] = colors[3][3] = 255;
                    }
                    else {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (col0[i] + col1[i]) / 2;
                        colors[2][3] = 255;
                        colors[3] = { 0, 0, 0, 0 };
                    }

                    uint32_t bits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
                    for (int py = 0; py < 4; ++py) {
                        for (int px = 0; px < 4; ++px) {
                            int bitIdx = 2 * (py * 4 + px);
                            uint8_t code = (bits >> bitIdx) & 0x3;
                            int tx = x + px;
                            int ty = y + py;
                            if (tx < width && ty < height) {
                                size_t dst = (ty * width + tx) * 4;
                                for (int c = 0; c < 4; ++c)
                                    rgba[dst + c] = colors[code][c];
                            }
                        }
                    }
                }
            }
            QString filename = QString("%1[%2]_dxt1.png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);
            uint32_t pitch = std::max<uint32_t>((width + 3) / 4, 1u) * 8;
            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_BC1_UNORM, pitch, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 844388420: // dxt2 premultiplied alpha
        {
            ptr = ptrBackup;
            size_t totalSize = 0;
            uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(header.nWidth, header.nHeight)))) + 1;
            uint32_t validMipLevels = 0;
            std::vector<std::vector<uint8_t>> mipData;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = ((mipW + 3) / 4) * ((mipH + 3) / 4) * 16;

                if (totalSize + size > static_cast<size_t>(data.size() - (ptr - raw)))
                    break;

                std::vector<uint8_t> mip(size);
                memcpy(mip.data(), ptr + totalSize, size);
                mipData.push_back(std::move(mip));
                totalSize += size;
                ++validMipLevels;
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
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* block = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            auto decodeColor = [](uint16_t c) -> std::array<uint8_t, 3> {
                return {
                    static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31),
                    static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63),
                    static_cast<uint8_t>((c & 0x1F) * 255 / 31)
                };
                };

            for (int y = 0; y < height; y += 4) {
                for (int x = 0; x < width; x += 4, block += 16) {
                    const uint8_t* alphaBlock = block;
                    const uint8_t* colorBlock = block + 8;

                    uint16_t color0 = colorBlock[0] | (colorBlock[1] << 8);
                    uint16_t color1 = colorBlock[2] | (colorBlock[3] << 8);
                    uint32_t colorBits = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (colorBlock[7] << 24);

                    auto col0 = decodeColor(color0);
                    auto col1 = decodeColor(color1);

                    std::array<std::array<uint8_t, 3>, 4> colors = { {
                        col0, col1,
                        {
                            static_cast<uint8_t>((2 * col0[0] + col1[0]) / 3),
                            static_cast<uint8_t>((2 * col0[1] + col1[1]) / 3),
                            static_cast<uint8_t>((2 * col0[2] + col1[2]) / 3)
                        },
                        {
                            static_cast<uint8_t>((col0[0] + 2 * col1[0]) / 3),
                            static_cast<uint8_t>((col0[1] + 2 * col1[1]) / 3),
                            static_cast<uint8_t>((col0[2] + 2 * col1[2]) / 3)
                        }
                    } };

                    for (int py = 0; py < 4; ++py) {
                        for (int px = 0; px < 4; ++px) {
                            int blockIndex = py * 4 + px;

                            uint8_t a4 = (alphaBlock[py * 2 + (px >= 2 ? 1 : 0)] >> ((px % 2) * 4)) & 0xF;
                            uint8_t alpha8 = (a4 << 4) | a4; // expand to 8 bits

                            uint8_t colorIdx = (colorBits >> (2 * blockIndex)) & 0x3;
                            int tx = x + px;
                            int ty = y + py;
                            if (tx < width && ty < height) {
                                size_t dst = (ty * width + tx) * 4;
                               
                                for (int c = 0; c < 3; ++c) { // un-premultiply RGB
                                    rgba[dst + c] = (alpha8 == 0) ? 0 :
                                        std::min(255, (colors[colorIdx][c] * 255) / alpha8);
                                }
                                rgba[dst + 3] = alpha8;
                            }
                        }
                    }
                }
            }

            QString filename = QString("%1[%2]_dxt2.png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            uint32_t pitch = ((width + 3) / 4) * 16;
            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_BC2_UNORM, pitch, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 861165636: // dxt3
        {
            ptr = ptrBackup;
            size_t totalSize = 0;
            uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(header.nWidth, header.nHeight)))) + 1;
            uint32_t validMipLevels = 0;
            std::vector<std::vector<uint8_t>> mipData;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = ((mipW + 3) / 4) * ((mipH + 3) / 4) * 16;

                if (totalSize + size > static_cast<size_t>(data.size() - (ptr - raw)))
                    break;

                std::vector<uint8_t> mip(size);
                memcpy(mip.data(), ptr + totalSize, size);
                mipData.push_back(std::move(mip));
                totalSize += size;
                ++validMipLevels;
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
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* block = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            auto decodeColor = [](uint16_t c) -> std::array<uint8_t, 3> {
                return {
                    static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31),
                    static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63),
                    static_cast<uint8_t>((c & 0x1F) * 255 / 31)
                };
                };

            for (int y = 0; y < height; y += 4) {
                for (int x = 0; x < width; x += 4, block += 16) {
                    const uint8_t* alphaBlock = block;
                    const uint8_t* colorBlock = block + 8;

                    uint16_t c0 = colorBlock[0] | (colorBlock[1] << 8);
                    uint16_t c1 = colorBlock[2] | (colorBlock[3] << 8);
                    auto col0 = decodeColor(c0);
                    auto col1 = decodeColor(c1);

                    std::array<std::array<uint8_t, 4>, 4> colors = { {
                        { col0[0], col0[1], col0[2], 255 },
                        { col1[0], col1[1], col1[2], 255 },
                        {}, {}
                    } };

                    if (c0 > c1) {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (2 * col0[i] + col1[i]) / 3;
                        for (int i = 0; i < 3; ++i)
                            colors[3][i] = (col0[i] + 2 * col1[i]) / 3;
                        colors[2][3] = colors[3][3] = 255;
                    }
                    else {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (col0[i] + col1[i]) / 2;
                        colors[2][3] = 255;
                        colors[3] = { 0, 0, 0, 0 };
                    }

                    uint32_t bits = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (colorBlock[7] << 24);

                    for (int py = 0; py < 4; ++py) {
                        uint16_t alphaRow = alphaBlock[py * 2] | (alphaBlock[py * 2 + 1] << 8);
                        for (int px = 0; px < 4; ++px) {
                            uint8_t alpha = ((alphaRow >> (px * 4)) & 0xF) * 17;
                            uint8_t code = (bits >> (2 * (py * 4 + px))) & 0x3;
                            int tx = x + px;
                            int ty = y + py;
                            if (tx < width && ty < height) {
                                size_t dst = (ty * width + tx) * 4;
                                for (int c = 0; c < 3; ++c)
                                    rgba[dst + c] = colors[code][c];
                                rgba[dst + 3] = alpha;
                            }
                        }
                    }
                }
            }

            QString filename = QString("%1[%2]_dxt3.png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            uint32_t pitch = ((width + 3) / 4) * 16;
            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_BC2_UNORM, pitch, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 877942852: // dxt4
        {
            ptr = ptrBackup;
            size_t totalSize = 0;
            uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(header.nWidth, header.nHeight)))) + 1;
            uint32_t validMipLevels = 0;
            std::vector<std::vector<uint8_t>> mipData;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = ((mipW + 3) / 4) * ((mipH + 3) / 4) * 16;

                if (totalSize + size > static_cast<size_t>(data.size() - (ptr - raw)))
                    break;

                std::vector<uint8_t> mip(size);
                memcpy(mip.data(), ptr + totalSize, size);
                mipData.push_back(std::move(mip));
                totalSize += size;
                ++validMipLevels;
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
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* block = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            auto decodeColor = [](uint16_t c) -> std::array<uint8_t, 3> {
                return {
                    static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31),
                    static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63),
                    static_cast<uint8_t>((c & 0x1F) * 255 / 31)
                };
                };

            for (int y = 0; y < height; y += 4) {
                for (int x = 0; x < width; x += 4, block += 16) {
                    uint8_t alpha0 = block[0];
                    uint8_t alpha1 = block[1];
                    const uint8_t* alphaBits = block + 2;

                    uint16_t c0 = block[8] | (block[9] << 8);
                    uint16_t c1 = block[10] | (block[11] << 8);
                    auto col0 = decodeColor(c0);
                    auto col1 = decodeColor(c1);

                    std::array<std::array<uint8_t, 4>, 4> colors = { {
                        { col0[0], col0[1], col0[2], 255 },
                        { col1[0], col1[1], col1[2], 255 },
                        {}, {}
                    } };

                    if (c0 > c1) {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (2 * col0[i] + col1[i]) / 3;
                        for (int i = 0; i < 3; ++i)
                            colors[3][i] = (col0[i] + 2 * col1[i]) / 3;
                        colors[2][3] = colors[3][3] = 255;
                    }
                    else {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (col0[i] + col1[i]) / 2;
                        colors[2][3] = 255;
                        colors[3] = { 0, 0, 0, 0 };
                    }

                    uint64_t alphaCode = 0;
                    for (int i = 5; i >= 0; --i)
                        alphaCode = (alphaCode << 8) | alphaBits[i];

                    uint8_t alphaTable[8];
                    alphaTable[0] = alpha0;
                    alphaTable[1] = alpha1;

                    if (alpha0 > alpha1) {
                        for (int i = 2; i < 8; ++i)
                            alphaTable[i] = static_cast<uint8_t>(((8 - i) * alpha0 + (i - 1) * alpha1) / 7);
                    }
                    else {
                        for (int i = 2; i < 6; ++i)
                            alphaTable[i] = static_cast<uint8_t>(((6 - i) * alpha0 + (i - 1) * alpha1) / 5);
                        alphaTable[6] = 0;
                        alphaTable[7] = 255;
                    }

                    uint32_t colorBits = block[12] | (block[13] << 8) | (block[14] << 16) | (block[15] << 24);

                    for (int py = 0; py < 4; ++py) {
                        for (int px = 0; px < 4; ++px) {
                            int alphaIdx = (alphaCode >> (3 * (py * 4 + px))) & 0x7;
                            int colorIdx = (colorBits >> (2 * (py * 4 + px))) & 0x3;
                            int tx = x + px;
                            int ty = y + py;
                            if (tx < width && ty < height) {
                                size_t dst = (ty * width + tx) * 4;
                                uint8_t a = alphaTable[alphaIdx];

                                
                                for (int c = 0; c < 3; ++c) // unpremultiply RGB for PNG export
                                    rgba[dst + c] = (a == 0) ? 0 : std::min(255, (colors[colorIdx][c] * 255) / a);
                                rgba[dst + 3] = a;
                            }
                        }
                    }
                }
            }

            QString filename = QString("%1[%2]_dxt4.png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            uint32_t pitch = ((width + 3) / 4) * 16;
            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_BC2_UNORM, pitch, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 894720068: // dxt5
        {
            ptr = ptrBackup;
            size_t totalSize = 0;
            uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(header.nWidth, header.nHeight)))) + 1;
            uint32_t validMipLevels = 0;
            std::vector<std::vector<uint8_t>> mipData;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = ((mipW + 3) / 4) * ((mipH + 3) / 4) * 16;

                if (totalSize + size > static_cast<size_t>(data.size() - (ptr - raw)))
                    break;

                std::vector<uint8_t> mip(size);
                if (header.szID[3] == 7) {
                    crypt.DecryptBuffer(mip.data(), size);
                }
                memcpy(mip.data(), ptr + totalSize, size);
                mipData.push_back(std::move(mip));
                totalSize += size;
                ++validMipLevels;
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
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* block = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            auto decodeColor = [](uint16_t c) -> std::array<uint8_t, 3> {
                return {
                    static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31),
                    static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63),
                    static_cast<uint8_t>((c & 0x1F) * 255 / 31)
                };
                };

            for (int y = 0; y < height; y += 4) {
                for (int x = 0; x < width; x += 4, block += 16) {
                    uint8_t alpha0 = block[0];
                    uint8_t alpha1 = block[1];
                    const uint8_t* alphaBits = block + 2;

                    uint64_t alphaCode = 0;
                    for (int i = 5; i >= 0; --i)
                        alphaCode = (alphaCode << 8) | alphaBits[i];

                    uint8_t alphaTable[8];
                    alphaTable[0] = alpha0;
                    alphaTable[1] = alpha1;

                    if (alpha0 > alpha1) {
                        for (int i = 2; i < 8; ++i)
                            alphaTable[i] = static_cast<uint8_t>(((8 - i) * alpha0 + (i - 1) * alpha1) / 7);
                    }
                    else {
                        for (int i = 2; i < 6; ++i)
                            alphaTable[i] = static_cast<uint8_t>(((6 - i) * alpha0 + (i - 1) * alpha1) / 5);
                        alphaTable[6] = 0;
                        alphaTable[7] = 255;
                    }

                    const uint8_t* colorBlock = block + 8;
                    uint16_t c0 = colorBlock[0] | (colorBlock[1] << 8);
                    uint16_t c1 = colorBlock[2] | (colorBlock[3] << 8);
                    auto col0 = decodeColor(c0);
                    auto col1 = decodeColor(c1);

                    std::array<std::array<uint8_t, 4>, 4> colors = { {
                        { col0[0], col0[1], col0[2], 255 },
                        { col1[0], col1[1], col1[2], 255 },
                        {}, {}
                    } };

                    if (c0 > c1) {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (2 * col0[i] + col1[i]) / 3;
                        for (int i = 0; i < 3; ++i)
                            colors[3][i] = (col0[i] + 2 * col1[i]) / 3;
                        colors[2][3] = colors[3][3] = 255;
                    }
                    else {
                        for (int i = 0; i < 3; ++i)
                            colors[2][i] = (col0[i] + col1[i]) / 2;
                        colors[2][3] = 255;
                        colors[3] = { 0, 0, 0, 0 };
                    }

                    uint32_t colorBits = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (colorBlock[7] << 24);

                    for (int py = 0; py < 4; ++py) {
                        for (int px = 0; px < 4; ++px) {
                            int alphaIdx = (alphaCode >> (3 * (py * 4 + px))) & 0x7;
                            int colorIdx = (colorBits >> (2 * (py * 4 + px))) & 0x3;
                            int tx = x + px;
                            int ty = y + py;
                            if (tx < width && ty < height) {
                                size_t dst = (ty * width + tx) * 4;
                                for (int c = 0; c < 3; ++c)
                                    rgba[dst + c] = colors[colorIdx][c];
                                rgba[dst + 3] = alphaTable[alphaIdx];
                            }
                        }
                    }
                }
            }

            QString filename = QString("%1[%2]_dxt5.png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            uint32_t pitch = ((width + 3) / 4) * 16;
            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_BC3_UNORM, pitch, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 20:
        {
            uint32_t mipLevels = floor(log2(std::max(header.nWidth, header.nHeight))) + 1;
            ptr = ptrBackup;

            for (uint32_t i = 0; i < mipLevels; ++i) 
            {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t mipSize = mipW * mipH * 3;

                if (ptr + totalSize + mipSize > raw + data.size())
                    break;

                totalSize += mipSize;
                ++validMipLevels;
            }

            ptr = ptrBackup;
            std::vector<std::vector<uint8_t>> mipData(validMipLevels);
            for (uint32_t i = 0; i < validMipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = mipW * mipH * 3;

                if (ptr + size > raw + data.size()) return result;

                mipData[i].resize(size);
                memcpy(mipData[i].data(), ptr, size);
                ptr += size;
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* src = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            for (size_t i = 0; i < width * height; ++i) {
                rgba[i * 4 + 0] = src[i * 3 + 2];
                rgba[i * 4 + 1] = src[i * 3 + 1];
                rgba[i * 4 + 2] = src[i * 3 + 0];
                rgba[i * 4 + 3] = 255;
            }

            QString filename = QString("%1[%2].png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            if (!CreateD3DTexture(device, rgba.data(), width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 4, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 21:
        {
            uint32_t mipLevels;
            mipLevels = floor(log2(std::max(header.nWidth, header.nHeight))) + 1;
            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t mipSize = mipW * mipH * 4;

                if (ptr + totalSize + mipSize > raw + data.size())
                    break;

                totalSize += mipSize;
                ++validMipLevels;
            }

            ptr = ptrBackup;

            std::vector<std::vector<uint8_t>> mipData(validMipLevels);

            for (uint32_t i = 0; i < validMipLevels; ++i)
            {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = mipW * mipH * 4;

                if (ptr + size > raw + data.size()) return result;

                mipData[i].resize(size);
                memcpy(mipData[i].data(), ptr, size);
                ptr += size;
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* src = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            for (size_t i = 0; i < width * height; ++i) {
                rgba[i * 4 + 0] = src[i * 4 + 2]; // R
                rgba[i * 4 + 1] = src[i * 4 + 1]; // G
                rgba[i * 4 + 2] = src[i * 4 + 0]; // B
                rgba[i * 4 + 3] = src[i * 4 + 3]; // A
            }

            QString filename = QString("%1[%2].png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);
            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_B8G8R8A8_UNORM, width * 4, &result.srv)) {
                return result;
            }
            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 22:
        {
            uint32_t mipLevels = floor(log2(std::max(header.nWidth, header.nHeight))) + 1;
            ptr = ptrBackup;
            uint32_t validMipLevels = 0;
            size_t totalSize = 0;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t mipSize = mipW * mipH * 4;

                if (ptr + totalSize + mipSize > raw + data.size())
                    break;

                totalSize += mipSize;
                ++validMipLevels;
            }

            ptr = ptrBackup;
            std::vector<std::vector<uint8_t>> mipData(validMipLevels);
            for (uint32_t i = 0; i < validMipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = mipW * mipH * 4;

                if (ptr + size > raw + data.size()) return result;

                mipData[i].resize(size);
                memcpy(mipData[i].data(), ptr, size);
                ptr += size;
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint8_t* src = mipData[0].data();
            std::vector<uint8_t> rgba(width * height * 4);

            for (size_t i = 0; i < width * height; ++i) {
                rgba[i * 4 + 0] = src[i * 4 + 2];
                rgba[i * 4 + 1] = src[i * 4 + 1];
                rgba[i * 4 + 2] = src[i * 4 + 0];
                rgba[i * 4 + 3] = 255;
            }

            QString filename = QString("%1[%2].png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_B8G8R8X8_UNORM, width * 4, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 23:
        {
            uint32_t mipLevels = floor(log2(std::max(header.nWidth, header.nHeight))) + 1;
            ptr = ptrBackup;
            uint32_t validMipLevels = 0;
            size_t totalSize = 0;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t mipSize = mipW * mipH * 4;

                if (ptr + totalSize + mipSize > raw + data.size())
                    break;

                totalSize += mipSize;
                ++validMipLevels;
            }

            ptr = ptrBackup;
            std::vector<std::vector<uint8_t>> mipData(validMipLevels);
            for (uint32_t i = 0; i < validMipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = mipW * mipH * 4;

                if (ptr + size > raw + data.size()) return result;

                mipData[i].resize(size);
                memcpy(mipData[i].data(), ptr, size);
                ptr += size;
            }

            int width = header.nWidth;
            int height = header.nHeight;

            QString filename = QString("%1[%2].png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, mipData[0].data(), width * 4);

            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 25:
        {
            uint32_t mipLevels = floor(log2(std::max(header.nWidth, header.nHeight))) + 1;
            ptr = ptrBackup;
            uint32_t validMipLevels = 0;
            size_t totalSize = 0;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t mipSize = mipW * mipH * 2;

                if (ptr + totalSize + mipSize > raw + data.size())
                    break;

                totalSize += mipSize;
                ++validMipLevels;
            }

            ptr = ptrBackup;
            std::vector<std::vector<uint8_t>> mipData(validMipLevels);
            for (uint32_t i = 0; i < validMipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = mipW * mipH * 2;

                if (ptr + size > raw + data.size()) return result;

                mipData[i].resize(size);
                memcpy(mipData[i].data(), ptr, size);
                ptr += size;
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint16_t* src = reinterpret_cast<const uint16_t*>(mipData[0].data());
            std::vector<uint8_t> rgba(width * height * 4);

            for (size_t i = 0; i < width * height; ++i) {
                uint16_t val = src[i];
                uint8_t a = (val >> 15) & 0x1;
                uint8_t r = (val >> 10) & 0x1F;
                uint8_t g = (val >> 5) & 0x1F;
                uint8_t b = val & 0x1F;

                rgba[i * 4 + 0] = (r << 3) | (r >> 2);  // R
                rgba[i * 4 + 1] = (g << 3) | (g >> 2);  // G
                rgba[i * 4 + 2] = (b << 3) | (b >> 2);  // B
                rgba[i * 4 + 3] = a ? 255 : 0;          // A
            }

            QString filename = QString("%1[%2].png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            if (!CreateD3DTexture(device, mipData[0].data(), width, height, DXGI_FORMAT_B5G5R5A1_UNORM, width * 2, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        case 26:
        {
            uint32_t mipLevels = floor(log2(std::max(header.nWidth, header.nHeight))) + 1;
            ptr = ptrBackup;
            uint32_t validMipLevels = 0;
            size_t totalSize = 0;

            for (uint32_t i = 0; i < mipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t mipSize = mipW * mipH * 2;

                if (ptr + totalSize + mipSize > raw + data.size())
                    break;

                totalSize += mipSize;
                ++validMipLevels;
            }

            ptr = ptrBackup;
            std::vector<std::vector<uint8_t>> mipData(validMipLevels);
            for (uint32_t i = 0; i < validMipLevels; ++i) {
                uint32_t mipW = std::max(header.nWidth >> i, 1u);
                uint32_t mipH = std::max(header.nHeight >> i, 1u);
                size_t size = mipW * mipH * 2;

                if (ptr + size > raw + data.size()) return result;

                mipData[i].resize(size);
                memcpy(mipData[i].data(), ptr, size);
                ptr += size;
            }

            int width = header.nWidth;
            int height = header.nHeight;
            const uint16_t* src = reinterpret_cast<const uint16_t*>(mipData[0].data());
            std::vector<uint8_t> rgba(width * height * 4);

            for (size_t i = 0; i < width * height; ++i) {
                uint16_t val = src[i];
                uint8_t a = (val >> 12) & 0xF;
                uint8_t r = (val >> 8) & 0xF;
                uint8_t g = (val >> 4) & 0xF;
                uint8_t b = val & 0xF;

                rgba[i * 4 + 0] = (r << 4) | r;
                rgba[i * 4 + 1] = (g << 4) | g;
                rgba[i * 4 + 2] = (b << 4) | b;
                rgba[i * 4 + 3] = (a << 4) | a;
            }

            QString filename = QString("%1[%2].png").arg(baseName).arg(index);
            stbi_write_png(filename.toStdString().c_str(), width, height, 4, rgba.data(), width * 4);

            if (!CreateD3DTexture(device, rgba.data(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM, width * 4, &result.srv)) {
                return result;
            }

            result.width = width;
            result.height = height;
            return result;
        }
        break;
        default:
            return result;
        }
    }



bool GTTLoader::CreateD3DTexture(ID3D11Device* device, const uint8_t* pixelData, uint32_t width, uint32_t height, DXGI_FORMAT format, int pitch, ID3D11ShaderResourceView** outSRV)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub = {};
    sub.pSysMem = pixelData;
    sub.SysMemPitch = pitch;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &sub, &texture))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    bool success = SUCCEEDED(device->CreateShaderResourceView(texture, &srvDesc, outSRV));
    texture->Release();
    return success;
}