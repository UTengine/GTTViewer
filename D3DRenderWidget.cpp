#include "stdafx.h"
#include "D3DRenderWidget.h"

D3DRenderWidget::D3DRenderWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    QTimer::singleShot(0, this, [this] {
        initializeD3D();
        });
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, QOverload<>::of(&D3DRenderWidget::render));
    timer->start(16);
}

D3DRenderWidget::~D3DRenderWidget() {
    cleanupD3D();
}

void D3DRenderWidget::initializeD3D() {
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = std::max(width(), 1);
    scd.BufferDesc.Height = std::max(height(), 1);
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = reinterpret_cast<HWND>(winId());
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &scd, &swapChain, &device, nullptr, &context);
    if (FAILED(hr)) {
        qDebug() << "D3D11CreateDeviceAndSwapChain FAILED:" << QString::number(hr, 16);
        return;
    }

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompileFromFile(L"texture_vertex.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return;
    }
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);

    ID3DBlob* psBlob = nullptr;
    hr = D3DCompileFromFile(L"texture_pixel.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return;
    }
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
    psBlob->Release();

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, u), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    vsBlob->Release();

    D3D11_SAMPLER_DESC samp = {}; // anisotropic filtering
    samp.Filter = D3D11_FILTER_ANISOTROPIC;
    samp.AddressU = samp.AddressV = samp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samp.MaxAnisotropy = 8;
    samp.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samp.MinLOD = 0;
    samp.MipLODBias = 0.0f;
    samp.MaxLOD = 1;  // Clamp to lod 0 because it still tries to sample invalid miplevels encrypted we will do it elsewhere too
    device->CreateSamplerState(&samp, &samplerState);

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.ByteWidth = sizeof(Vertex) * 6;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Vertex empty[6] = {};
    D3D11_SUBRESOURCE_DATA init = { empty };
    device->CreateBuffer(&bd, &init, &vertexBuffer);

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();

    D3D11_VIEWPORT vp = { 0 };
    vp.Width = static_cast<float>(width());
    vp.Height = static_cast<float>(height());
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    context->RSSetViewports(1, &vp);
}

void D3DRenderWidget::cleanupD3D() {
    if (renderTargetView) renderTargetView->Release();
    if (swapChain) swapChain->Release();
    if (context) context->Release();
    if (device) device->Release();
    if (vertexBuffer) vertexBuffer->Release();
    if (inputLayout) inputLayout->Release();
    if (vertexShader) vertexShader->Release();
    if (pixelShader) pixelShader->Release();
    if (samplerState) samplerState->Release();
    for (auto& tex : textures) if (tex.srv) tex.srv->Release();
}

void D3DRenderWidget::paintEvent(QPaintEvent*) {
}

void D3DRenderWidget::resizeEvent(QResizeEvent*) {
    if (!swapChain) return;
    if (renderTargetView) renderTargetView->Release();
    swapChain->ResizeBuffers(0, width(), height(), DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();

    D3D11_VIEWPORT vp = { 0 };
    vp.Width = static_cast<float>(width());
    vp.Height = static_cast<float>(height());
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    context->RSSetViewports(1, &vp);
}

void D3DRenderWidget::render() {
    if (!context || !renderTargetView) return;

    float clearColor[] = { 0.1f, 0.1f, 0.3f, 1.f };
    context->ClearRenderTargetView(renderTargetView, clearColor);

    context->OMSetRenderTargets(1, &renderTargetView, nullptr);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);
    context->PSSetSamplers(0, 1, &samplerState);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    float screenHeight = static_cast<float>(height());
    float screenWidth = static_cast<float>(width());

    float yCursor = 1.0f;
    for (size_t i = 0; i < textures.size(); ++i) {
        float texW = static_cast<float>(textures[i].width);
        float texH = static_cast<float>(textures[i].height);

        float ndcW = (texW / screenWidth) * 2.0f * zoom;
        float ndcH = (texH / screenHeight) * 2.0f * zoom;

        float left = -ndcW / 2.0f + panOffset.x();
        float right = ndcW / 2.0f + panOffset.x();
        float top = yCursor + panOffset.y();
        float bottom = top - ndcH;

        setQuadVertices(left, right, top, bottom);
        yCursor -= ndcH;

        D3D11_MAPPED_SUBRESOURCE mapped;
        context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, quadVertices, sizeof(quadVertices));
        context->Unmap(vertexBuffer, 0);

        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        context->PSSetShaderResources(0, 1, &textures[i].srv);
        context->Draw(6, 0);
    }
    swapChain->Present(1, 0);
}

void D3DRenderWidget::setQuadVertices(float l, float r, float t, float b) {
    quadVertices[0] = { l, t, 0.f, 0.f, 0.f };
    quadVertices[1] = { r, t, 0.f, 1.f, 0.f };
    quadVertices[2] = { l, b, 0.f, 0.f, 1.f };
    quadVertices[3] = { r, t, 0.f, 1.f, 0.f };
    quadVertices[4] = { r, b, 0.f, 1.f, 1.f };
    quadVertices[5] = { l, b, 0.f, 0.f, 1.f };
}

void D3DRenderWidget::loadTextures(const QByteArray& rawData, const QVector<qint64>& offsets) {
    textures.clear();

    for (qint64 offset : offsets) {
        LoadedTexture tex = GTTLoader::LoadTextureAtOffset(device, rawData, offset);
        if (tex.srv) {
            textures.push_back(tex);

            QString succMsg = QString("Loaded texture: %1 x %2 at offset: %3 , Hex Address Go to offset: (0x%4)")
                .arg(tex.width)
                .arg(tex.height)
                .arg(offset)
                .arg(offset, 0, 16);

#ifdef _DEBUG
            qDebug() << succMsg;
#else
            DebugLogger::log(succMsg);
#endif
        }
        else {
            QString failMsg = QString("FAILED to load texture at offset: %1 (0x%2)")
                .arg(offset)
                .arg(offset, 0, 16);
#ifdef _DEBUG
            qDebug() << failMsg;
#else
            DebugLogger::log(failMsg);
#endif
        }
    }
}


void D3DRenderWidget::mousePressEvent(QMouseEvent* event) {
    lastMousePos = event->pos();
}

void D3DRenderWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint delta = event->pos() - lastMousePos;
    lastMousePos = event->pos();
    float dx = 2.0f * delta.x() / width();
    float dy = -2.0f * delta.y() / height();
    panOffset += QPointF(dx, dy) / zoom;
}

void D3DRenderWidget::wheelEvent(QWheelEvent* event) {
    float oldZoom = zoom;
    float zoomFactor = (event->angleDelta().y() > 0) ? 1.1f : 0.9f;
    zoom *= zoomFactor;
    zoom = std::clamp(zoom, 0.1f, 10.0f);
    float x = event->position().x() / width();
    float y = event->position().y() / height();
    float ndcX = x * 2.0f - 1.0f;
    float ndcY = 1.0f - y * 2.0f;
    QPointF before = (QPointF(ndcX, ndcY) - panOffset) / oldZoom;
    QPointF after = (QPointF(ndcX, ndcY) - panOffset) / zoom;
    panOffset += (after - before) * zoom;
}