#pragma once
#include <QWidget>
#include <d3d11.h>
#include "GTTLoader.h"

struct Vertex {
    float x, y, z;
    float u, v;
};

class D3DRenderWidget : public QWidget {
    Q_OBJECT
public:
    explicit D3DRenderWidget(QWidget* parent = nullptr);
    ~D3DRenderWidget();

    float zoom = 1.0f;
    QPoint lastMousePos;
    bool dragging = false;
    QPointF panOffset = { 0.f, 0.f };

    void loadTextures(const QByteArray& rawData, const QVector<qint64>& offsets, const QString& baseName);
    ID3D11Device* getDevice() const { return device; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    QPaintEngine* paintEngine() const override { return nullptr; }

private:
    void initializeD3D();
    void cleanupD3D();
    void render();
    void setQuadVertices(float l, float r, float t, float b);

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    std::vector<LoadedTexture> textures;
    Vertex quadVertices[6];
};
