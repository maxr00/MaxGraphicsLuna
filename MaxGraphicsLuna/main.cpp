#include <d3dx11effect.h>
#include "Engine\d3dApp.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

class BoyEngine : public D3DApp
{
	ID3D11Buffer* mBoxVB;
	ID3D11Buffer* mBoxIB;

	ID3DX11Effect* mFX;
	ID3DX11EffectTechnique* mTech;
	ID3DX11EffectMatrixVariable* mfxWorldViewProj;

	ID3D11InputLayout* mInputLayout;

	XMFLOAT4X4 mWorld;
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;

	float mTheta;
	float mPhi;
	float mRadius;

	POINT mLastMousePos;

	Assimp::Importer importer;

	const aiScene* loadedModel;

public:
	BoyEngine(HINSTANCE hInstance)
		: D3DApp(hInstance),
		mBoxVB(0), mBoxIB(0), mFX(0), mTech(0), mfxWorldViewProj(0), mInputLayout(0),
		mTheta(1.5f * MathHelper::Pi),
		mPhi(0.25f*MathHelper::Pi),
		mRadius(5.0f)
	{
		mMainWndCaption = L"<3";

		mLastMousePos.x = 0;
		mLastMousePos.y = 0;

		XMMATRIX I = XMMatrixIdentity();
		XMStoreFloat4x4(&mWorld, I);
		XMStoreFloat4x4(&mView, I);
		XMStoreFloat4x4(&mProj, I);

		loadedModel = importer.ReadFile("Models/donut.fbx", aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

		if(!loadedModel)
			mMainWndCaption = L"!";
	}

	virtual ~BoyEngine()
	{
		ReleaseCOM(mBoxVB);
		ReleaseCOM(mBoxIB);
		ReleaseCOM(mFX);
		ReleaseCOM(mInputLayout);
	}

	virtual bool Init()
	{
		if (!D3DApp::Init())
			return false;

		BuildGeometryBuffers();
		BuildFX();
		BuildVertexLayout();

		return true;
	}

	virtual void OnResize()
	{
		D3DApp::OnResize();
		
		XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
		XMStoreFloat4x4(&mProj, P);
	}

	virtual void UpdateScene(float dt)
	{
		float x = mRadius * sinf(mPhi) * cosf(mTheta);
		float y = mRadius * cosf(mPhi);
		float z = mRadius * sinf(mPhi) * sinf(mTheta);

		XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
		XMVECTOR target = XMVectorZero();
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);

		XMMATRIX V = XMMatrixLookAtLH(pos, target, up);
		XMStoreFloat4x4(&mView, V);
	}

	virtual void DrawScene()
	{
		// Clear screen and depth/stencil
		md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::Blue));
		md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

		// Setup box stuff
		md3dImmediateContext->IASetInputLayout(mInputLayout);
		md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		md3dImmediateContext->IASetVertexBuffers(0, 1, &mBoxVB, &stride, &offset);
		md3dImmediateContext->IASetIndexBuffer(mBoxIB, DXGI_FORMAT_R32_UINT, 0);

		//  Set constants
		XMMATRIX world = XMLoadFloat4x4(&mWorld);
		XMMATRIX view = XMLoadFloat4x4(&mView);
		XMMATRIX proj = XMLoadFloat4x4(&mProj);
		XMMATRIX worldViewProj = world * view * proj;

		mfxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&worldViewProj));

		// Render the boy
		D3DX11_TECHNIQUE_DESC techDesc;
		mTech->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; p++)
		{
			mTech->GetPassByIndex(0)->Apply(0, md3dImmediateContext);

			// 36 indices for the box
			md3dImmediateContext->DrawIndexed(loadedModel->mMeshes[0]->mNumFaces * 3, 0, 0);
		}

		// Present to screen
		HR(mSwapChain->Present(0, 0));
	}

	void OnMouseDown(WPARAM btnState, int x, int y)
	{
		mLastMousePos.x = x;
		mLastMousePos.y = y;

		SetCapture(mhMainWnd);
	}

	void OnMouseUp(WPARAM btnState, int x, int y)
	{
		ReleaseCapture();
	}

	void OnMouseMove(WPARAM btnState, int x, int y)
	{
		if ((btnState & MK_LBUTTON) != 0) // Check for left click
		{
			// Make each pixel correspond to a quarter of a degree
			float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

			mTheta -= dx;
			mPhi -= dy;

			mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
		}
		else if ((btnState & MK_RBUTTON) != 0)
		{
			float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
			float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

			mRadius += dx - dy;

			mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
		}

		mLastMousePos.x = x;
		mLastMousePos.y = y;
	}

	void BuildGeometryBuffers()
	{

		std::vector<Vertex> vertices = std::vector<Vertex>(loadedModel->mMeshes[0]->mNumVertices);

		for (int i = 0; i < loadedModel->mMeshes[0]->mNumVertices; i++)
		{
			vertices[i].Pos = XMFLOAT3(loadedModel->mMeshes[0]->mVertices[i].x, loadedModel->mMeshes[0]->mVertices[i].y, loadedModel->mMeshes[0]->mVertices[i].z);

			vertices[i].Color = (const float*)&Colors::Yellow;
		}

		/*
		Vertex vertices[] = 
		{
			{ XMFLOAT3(-1.0f, -1.0f, -1.0f), (const float*)&Colors::White },
			{ XMFLOAT3(-1.0f, +1.0f, -1.0f), (const float*)&Colors::Black },
			{ XMFLOAT3(+1.0f, +1.0f, -1.0f), (const float*)&Colors::Red },
			{ XMFLOAT3(+1.0f, -1.0f, -1.0f), (const float*)&Colors::Green },
			{ XMFLOAT3(-1.0f, -1.0f, +1.0f), (const float*)&Colors::Blue },
			{ XMFLOAT3(-1.0f, +1.0f, +1.0f), (const float*)&Colors::Yellow },
			{ XMFLOAT3(+1.0f, +1.0f, +1.0f), (const float*)&Colors::Cyan },
			{ XMFLOAT3(+1.0f, -1.0f, +1.0f), (const float*)&Colors::Magenta }
		};
		*/

		D3D11_BUFFER_DESC vbd;
		vbd.Usage = D3D11_USAGE_IMMUTABLE;
		vbd.ByteWidth = sizeof(Vertex) * loadedModel->mMeshes[0]->mNumVertices;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = 0;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA vinitData;
		vinitData.pSysMem = vertices.data();
		HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &mBoxVB));

		/*
		// index buffer
		UINT indices[] = 
		{
			// Front
			0,1,2,
			0,2,3,
			// Back
			4,6,5,
			4,7,6,
			// Left
			4,5,1,
			4,1,0,
			// Right
			3,2,6,
			3,6,7,
			// Top
			1,5,6,
			1,6,2,
			// Bottom
			4,0,3,
			4,3,7
		};
		*/
		std::vector<UINT> indices = std::vector<UINT>(loadedModel->mMeshes[0]->mNumFaces * 3);

		for (int i = 0; i < loadedModel->mMeshes[0]->mNumFaces; i++)
		{
			indices[i * 3 + 0] = loadedModel->mMeshes[0]->mFaces[i].mIndices[0];
			indices[i * 3 + 1] = loadedModel->mMeshes[0]->mFaces[i].mIndices[1];
			indices[i * 3 + 2] = loadedModel->mMeshes[0]->mFaces[i].mIndices[2];
		}
		

		D3D11_BUFFER_DESC ibd;
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = sizeof(UINT) * loadedModel->mMeshes[0]->mNumFaces * 3;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = 0;
		ibd.MiscFlags = 0;
		ibd.StructureByteStride = 0;
		
		D3D11_SUBRESOURCE_DATA iinitData;
		iinitData.pSysMem = indices.data();
		HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &mBoxIB));
	}

	void BuildFX()
	{
		DWORD shaderFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
		shaderFlags |= D3D10_SHADER_DEBUG;
		shaderFlags |= D3D10_SHADER_SKIP_OPTIMIZATION;
#endif

		ID3D10Blob* compiledShader = 0;
		ID3D10Blob* compilationMsgs = 0;

		// Compile shader
		HRESULT hr = D3DX11CompileFromFile("FX/color.fx", 0, 0, 0, "fx_5_0", shaderFlags, 0, 0, &compiledShader, &compilationMsgs, 0);

		if (compilationMsgs != 0)
		{
			MessageBoxA(0, (char*)compilationMsgs->GetBufferPointer(), 0, 0);
			ReleaseCOM(compilationMsgs);
		}

		if (FAILED(hr))
		{
			DXTrace(__FILE__, (DWORD)__LINE__, hr, "D3DX11CompileFromFile", true);
		}

		// Create the effect
		HR(D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), 0, md3dDevice, &mFX));

		ReleaseCOM(compiledShader);

		mTech = mFX->GetTechniqueByName("ColorTech");
		mfxWorldViewProj = mFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	}

	void BuildVertexLayout()
	{
		D3D11_INPUT_ELEMENT_DESC vertexDesc[] = 
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};

		D3DX11_PASS_DESC passDesc;
		mTech->GetPassByIndex(0)->GetDesc(&passDesc);
		HR(md3dDevice->CreateInputLayout(vertexDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &mInputLayout));
	}

};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	BoyEngine app(hInstance);

	if (!app.Init())
		return 0;

	return app.Run();
}
