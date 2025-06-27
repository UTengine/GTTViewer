#pragma once

struct LoadedTexture {
    ID3D11ShaderResourceView* srv;
    uint32_t width;
    uint32_t height;
};

struct NTF3Header {
    char     szID[4];
    uint32_t nWidth;
    uint32_t nHeight;
    uint32_t Format;
    BOOL     bMipMap;
};

class GTTLoader {
public:
    static LoadedTexture LoadTextureAtOffset(ID3D11Device* device, const QByteArray& data, qint64 offset, const QString& baseName, uint32_t index);

private:
    //static DXGI_FORMAT ConvertToDXGIFormat(uint32_t d3dFormat);
    //static uint32_t CalculateMipLevels(uint32_t width, uint32_t height, bool hasMips);
    //static std::vector<uint8_t> ConvertA8R8G8B8ToB8G8R8A8(const uint8_t* src, uint32_t width, uint32_t height);
    //static std::vector<uint8_t> ConvertA1R5G5B5ToR8G8B8A8(const uint8_t* src, uint32_t width, uint32_t height);
    static bool CreateD3DTexture(ID3D11Device* device, const uint8_t* pixelData, uint32_t width, uint32_t height, DXGI_FORMAT format, int pitch, ID3D11ShaderResourceView** outSRV);
};
