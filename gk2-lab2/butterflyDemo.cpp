#include "butterflyDemo.h"

using namespace mini;
using namespace gk2;
using namespace DirectX;
using namespace std;

#pragma region Constants
const float ButterflyDemo::DODECAHEDRON_R = sqrtf(0.375f + 0.125f * sqrtf(5.0f));
const float ButterflyDemo::DODECAHEDRON_H = 1.0f + 2.0f * DODECAHEDRON_R;
const float ButterflyDemo::DODECAHEDRON_A = XMScalarACos(-0.2f * sqrtf(5.0f));

const float ButterflyDemo::MOEBIUS_R = 1.0f;
const float ButterflyDemo::MOEBIUS_W = 0.1f;
const unsigned int ButterflyDemo::MOEBIUS_N = 128;

const float ButterflyDemo::LAP_TIME = 10.0f;
const float ButterflyDemo::FLAP_TIME = 2.0f;
const float ButterflyDemo::WING_W = 0.15f;
const float ButterflyDemo::WING_H = 0.1f;
const float ButterflyDemo::WING_MAX_A = 8.0f * XM_PIDIV2 / 9.0f; //80 degrees

const unsigned int ButterflyDemo::BS_MASK = 0xffffffff;

const XMFLOAT4 ButterflyDemo::GREEN_LIGHT_POS = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
const XMFLOAT4 ButterflyDemo::BLUE_LIGHT_POS = XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f);
const XMFLOAT4 ButterflyDemo::COLORS[] = {
	XMFLOAT4(253.0f / 255.0f, 198.0f / 255.0f, 137.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(255.0f / 255.0f, 247.0f / 255.0f, 153.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(196.0f / 255.0f, 223.0f / 255.0f, 155.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(162.0f / 255.0f, 211.0f / 255.0f, 156.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(130.0f / 255.0f, 202.0f / 255.0f, 156.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(122.0f / 255.0f, 204.0f / 255.0f, 200.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(109.0f / 255.0f, 207.0f / 255.0f, 246.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(125.0f / 255.0f, 167.0f / 255.0f, 216.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(131.0f / 255.0f, 147.0f / 255.0f, 202.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(135.0f / 255.0f, 129.0f / 255.0f, 189.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(161.0f / 255.0f, 134.0f / 255.0f, 190.0f / 255.0f, 100.0f / 255.0f),
	XMFLOAT4(244.0f / 255.0f, 154.0f / 255.0f, 193.0f / 255.0f, 100.0f / 255.0f)
};
#pragma endregion

#pragma region Initalization
ButterflyDemo::ButterflyDemo(HINSTANCE hInstance)
	: Base(hInstance, 1280, 720, L"Motyl"),
	  m_cbWorld(m_device.CreateConstantBuffer<XMFLOAT4X4>()),
	  m_cbView(m_device.CreateConstantBuffer<XMFLOAT4X4, 2>()),
	  m_cbLighting(m_device.CreateConstantBuffer<Lighting>()),
	  m_cbSurfaceColor(m_device.CreateConstantBuffer<XMFLOAT4>())

{
	//Projection matrix
	auto s = m_window.getClientSize();
	auto ar = static_cast<float>(s.cx) / s.cy;
	XMStoreFloat4x4(&m_projMtx, XMMatrixPerspectiveFovLH(XM_PIDIV4, ar, 0.01f, 100.0f));
	m_cbProj = m_device.CreateConstantBuffer<XMFLOAT4X4>();
	UpdateBuffer(m_cbProj, m_projMtx);
	XMFLOAT4X4 cameraMtx;
	XMStoreFloat4x4(&cameraMtx, m_camera.getViewMatrix());
	UpdateCameraCB(cameraMtx);

	//Regular shaders
	auto vsCode = m_device.LoadByteCode(L"vs.cso");
	auto psCode = m_device.LoadByteCode(L"ps.cso");
	m_vs = m_device.CreateVertexShader(vsCode);
	m_ps = m_device.CreatePixelShader(psCode);

	//TODO : 0.3. Change input layout to match new vertex structure
	//m_il = m_device.CreateInputLayout(VertexPositionColor::Layout, vsCode);
	m_il = m_device.CreateInputLayout(VertexPositionNormal::Layout, vsCode);

	//Billboard shaders
	vsCode = m_device.LoadByteCode(L"vsBillboard.cso");
	psCode = m_device.LoadByteCode(L"psBillboard.cso");
	m_vsBillboard = m_device.CreateVertexShader(vsCode);
	m_psBillboard = m_device.CreatePixelShader(psCode);
	D3D11_INPUT_ELEMENT_DESC elements[1] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	m_ilBillboard = m_device.CreateInputLayout(elements, vsCode);

	//Render states
	CreateRenderStates();

	//Meshes

	//TODO : 0.2. Create a shaded box model instead of colored one 
	//m_box = Mesh::ColoredBox(m_device);
	m_box = Mesh::ShadedBox(m_device);

	m_pentagon = Mesh::Pentagon(m_device);
	m_wing = Mesh::DoubleRect(m_device, 0.3f);
	//m_wing = Mesh::DoubleRect(m_device, 2.0f, 1.0f);
	CreateMoebuisStrip();

	m_bilboard = Mesh::Billboard(m_device, 2.0f);

	//Model matrices
	CreateDodecahadronMtx();

	SetShaders();
	ID3D11Buffer* vsb[] = { m_cbWorld.get(),  m_cbView.get(), m_cbProj.get() };
	m_device.context()->VSSetConstantBuffers(0, 3, vsb);
	ID3D11Buffer* psb[] = { m_cbSurfaceColor.get(), m_cbLighting.get() };
	m_device.context()->PSSetConstantBuffers(0, 2, psb);
}

void ButterflyDemo::CreateRenderStates()
//Setup render states used in various stages of the scene rendering
{
	DepthStencilDescription dssDesc;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; //Disable writing to depth buffer
	m_dssNoDepthWrite = m_device.CreateDepthStencilState(dssDesc); // depth stencil state for billboards

	//TODO : 1.20. Setup depth stencil state for writing to stencil buffer
	dssDesc.StencilEnable = true;
	dssDesc.FrontFace.StencilPassOp = dssDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	m_dssStencilWrite = m_device.CreateDepthStencilState(dssDesc);

	//TODO : 1.36. Setup depth stencil state for stencil test for billboards
	dssDesc.BackFace.StencilFunc = dssDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	dssDesc.FrontFace.StencilPassOp = dssDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	m_dssStencilTestNoDepthWrite = m_device.CreateDepthStencilState(dssDesc);

	//TODO : 1.21. Setup depth stencil state for stencil test for 3D objects
	dssDesc.BackFace.StencilFunc = dssDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	dssDesc.FrontFace.StencilPassOp = dssDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP; // not needed
	m_dssStencilTest = m_device.CreateDepthStencilState(dssDesc);

	RasterizerDescription rsDesc;
	//TODO : 1.13. Setup rasterizer state with ccw front faces
	rsDesc.FrontCounterClockwise = true;
	m_rsCCW = m_device.CreateRasterizerState(rsDesc);

	BlendDescription bsDesc;
	//TODO : 1.26. Setup alpha blending state
	bsDesc.RenderTarget[0].BlendEnable = true;
	bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	m_bsAlpha = m_device.CreateBlendState(bsDesc);

	//TODO : 1.30. Setup additive blending state
	bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	m_bsAdd = m_device.CreateBlendState(bsDesc);
}

void ButterflyDemo::CreateDodecahadronMtx()
//Compute dodecahedronMtx and mirrorMtx
{
	//TODO : 1.01. calculate m_dodecahedronMtx matrices
	DirectX::XMStoreFloat4x4(&m_dodecahedronMtx[0],
		DirectX::XMMatrixRotationX(XM_PIDIV2) * DirectX::XMMatrixTranslation(0, -DODECAHEDRON_H / 2.0, 0));

	DirectX::XMStoreFloat4x4(&m_dodecahedronMtx[1],
		DirectX::XMLoadFloat4x4(&m_dodecahedronMtx[0]) * DirectX::XMMatrixRotationY(XM_PI) * DirectX::XMMatrixRotationZ(DODECAHEDRON_A - XM_PI));
	for (int i = 2; i < 6; i++) {
		DirectX::XMStoreFloat4x4(&m_dodecahedronMtx[i],
			DirectX::XMLoadFloat4x4(&m_dodecahedronMtx[i - 1]) * DirectX::XMMatrixRotationY(XM_2PI/5.0));
	}

	for (int i = 6; i < 12; i++) {
		DirectX::XMStoreFloat4x4(&m_dodecahedronMtx[i],
			DirectX::XMLoadFloat4x4(&m_dodecahedronMtx[i - 6]) * DirectX::XMMatrixRotationZ(XM_PI));
	}

	float scale = 3.0;
	for (int i = 0; i < 12; i++) {
		DirectX::XMStoreFloat4x4(&m_dodecahedronMtx[i],
			DirectX::XMLoadFloat4x4(&m_dodecahedronMtx[i]) * DirectX::XMMatrixScaling(scale, scale, scale));
	}

	//TODO : 1.12. calculate m_mirrorMtx matrices
	for (int i = 0; i < 12; i++) {
		XMMATRIX D = XMLoadFloat4x4(m_dodecahedronMtx + i);
		XMMATRIX Dinv = XMMatrixInverse(nullptr, D);
		XMMATRIX I = XMMatrixScaling(1, 1, -1);
		XMStoreFloat4x4(m_mirrorMtx + i, Dinv * I * D);
	}
}

XMFLOAT3 ButterflyDemo::MoebiusStripPos(float t, float s)
//TODO : 1.04. Compute the position of point on the Moebius strip for parameters t and s
{
	return {
		cosf(t) * (MOEBIUS_R + MOEBIUS_W * s * cosf(0.5f * t)),
		sinf(t) * (MOEBIUS_R + MOEBIUS_W * s * cosf(0.5f * t)),
		MOEBIUS_W * s * sinf(0.5f * t)
	};
}

XMVECTOR ButterflyDemo::MoebiusStripDs(float t, float s)
//TODO : 1.05. Return the s-derivative of point on the Moebius strip for parameters t and s
{
	return {
		cosf(0.5f * t) * cosf(t),
		cosf(0.5f * t) * sinf(t),
		sinf(0.5f * t)
	};
}

XMVECTOR ButterflyDemo::MoebiusStripDt(float t, float s)
//TODO : 1.06. Compute the t-derivative of point on the Moebius strip for parameters t and s
{
	return {
		-MOEBIUS_R * sinf(t) - 0.5f * s * MOEBIUS_W * sinf(0.5f * t) * cosf(t) - MOEBIUS_W * s * cosf(0.5f * t) * sinf(t),
		MOEBIUS_R * cosf(t) - 0.5f * s * MOEBIUS_W * sinf(0.5f * t) * sinf(t) + MOEBIUS_W * s * cosf(0.5f * t) * cosf(t),
		0.5f * s * MOEBIUS_W * cosf(t)
	};
}

void ButterflyDemo::CreateMoebuisStrip()
//TODO : 1.07. Create Moebius strip mesh
{
	const float t_step = XM_2PI / MOEBIUS_N;
	float t = 0.0f;
	int id = 0;

	std::vector<VertexPositionNormal> verts;
	std::vector<unsigned short> idxs;

	for (int j = 0; j < 2; j++, t = XM_2PI) {
		for (int i = 0; i < MOEBIUS_N; i++, t += t_step, id += 2) {
			// p1
			XMFLOAT3 position = MoebiusStripPos(t, 1.0f);
			XMVECTOR p_t = MoebiusStripDt(t, 1.0f);
			XMVECTOR p_s = MoebiusStripDs(t, 1.0f);
			XMVECTOR normal_vec = XMVector3Normalize(XMVector3Cross(p_s, p_t));
			XMFLOAT3 normal_fl;
			XMStoreFloat3(&normal_fl, normal_vec);
			verts.emplace_back(position, normal_fl);

			// p2
			position = MoebiusStripPos(t, -1.0f);
			p_t = MoebiusStripDt(t, -1.0f);
			p_s = MoebiusStripDs(t, -1.0f);
			normal_vec = XMVector3Normalize(XMVector3Cross(p_s, p_t));
			XMStoreFloat3(&normal_fl, normal_vec);
			verts.emplace_back(position, normal_fl);

			// tris
			idxs.emplace_back(id); idxs.emplace_back(id + 2); idxs.emplace_back(id + 1);
			idxs.emplace_back(id + 1); idxs.emplace_back(id + 2); idxs.emplace_back(id + 3);
		}
	}
	unsigned short len = idxs.size();
	idxs[len - 5] = 0; idxs[len - 2] = 0; idxs[len - 1] = 1;

	m_moebius = Mesh::SimpleTriMesh(m_device, verts, idxs);
}
#pragma endregion

#pragma region Per-Frame Update
void ButterflyDemo::Update(const Clock& c)
{
	Base::Update(c);
	double dt = c.getFrameTime();
	if (HandleCameraInput(dt))
	{
		XMFLOAT4X4 cameraMtx;
		XMStoreFloat4x4(&cameraMtx, m_camera.getViewMatrix());
		UpdateCameraCB(cameraMtx);
	}
	UpdateButterfly(static_cast<float>(dt));
}

void ButterflyDemo::UpdateCameraCB(DirectX::XMFLOAT4X4 cameraMtx)
{
	XMMATRIX mtx = XMLoadFloat4x4(&cameraMtx);
	XMVECTOR det;
	auto invvmtx = XMMatrixInverse(&det, mtx);
	XMFLOAT4X4 view[2] = { cameraMtx };
	XMStoreFloat4x4(view + 1, invvmtx);
	UpdateBuffer(m_cbView, view);
}

void ButterflyDemo::UpdateButterfly(float dtime)
//TODO : 1.10. Compute the matrices for butterfly wings. Position on the strip is determined based on time
{
	static float lap = 0.0f;
	lap += dtime;
	while (lap > LAP_TIME)
		lap -= LAP_TIME;
	//Value of the Moebius strip t parameter
	float t = 2 * lap / LAP_TIME;
	//Angle between wing current and vertical position
	float a = t * WING_MAX_A;
	t *= XM_2PI;
	if (a > WING_MAX_A)
		a = 2 * WING_MAX_A - a;
	//Write the rest of code here
	XMFLOAT3 p = MoebiusStripPos(t, 0);
	XMVECTOR P = XMLoadFloat3(&p);
	P.m128_f32[3] = 1.0f;
	XMVECTOR T = XMVector3Normalize(MoebiusStripDt(t, 0));
	XMVECTOR B = XMVector3Normalize(MoebiusStripDs(t, 0));
	XMVECTOR N = XMVector3Normalize(XMVector3Cross(B, T));
	XMMATRIX M(B, N, T, P);
	XMStoreFloat4x4(m_wingMtx, XMMatrixRotationY(XM_PIDIV2) * XMMatrixRotationZ(a) * XMMatrixTranslation(0,0.01,0) * M);
	XMStoreFloat4x4(m_wingMtx + 1, XMMatrixRotationY(XM_PIDIV2) * XMMatrixRotationZ(-a) * XMMatrixTranslation(0, 0.01, 0) * M);
}
#pragma endregion

#pragma region Frame Rendering Setup
void ButterflyDemo::SetShaders()
{
	m_device.context()->VSSetShader(m_vs.get(), 0, 0);
	m_device.context()->PSSetShader(m_ps.get(), 0, 0);
	m_device.context()->IASetInputLayout(m_il.get());
	m_device.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ButterflyDemo::SetBillboardShaders()
{
	m_device.context()->VSSetShader(m_vsBillboard.get(), 0, 0);
	m_device.context()->PSSetShader(m_psBillboard.get(), 0, 0);
	m_device.context()->IASetInputLayout(m_ilBillboard.get());
	m_device.context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ButterflyDemo::Set1Light()
//Setup one positional light at the camera
{
	Lighting l{
		/*.ambientColor = */ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		/*.surface = */ XMFLOAT4(0.2f, 0.4f, 0.8f, 200.0f),
		/*.lights =*/ {
			{ /*.position =*/ m_camera.getCameraPosition(), /*.color =*/ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }
			//other 2 lights set to 0
		}
	};
	UpdateBuffer(m_cbLighting, l);
}

void ButterflyDemo::Set3Lights()
//Setup one white positional light at the camera
//TODO : 1.28. Setup two additional positional lights, green and blue.
{
	Lighting l{
		/*.ambientColor = */ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		/*.surface = */ XMFLOAT4(0.2f, 0.4f, 0.8f, 200.0f),
		/*.lights =*/{
			{ /*.position =*/ m_camera.getCameraPosition(), /*.color =*/ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			//Write the rest of the code here
			{/*.position =*/ GREEN_LIGHT_POS, /*.color =*/ XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
			{/*.position =*/ BLUE_LIGHT_POS, /*.color =*/ XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }
		}
	};

	//comment the following line when structure is filled
	//ZeroMemory(&l.lights[1], sizeof(Light) * 2);

	UpdateBuffer(m_cbLighting, l);
}
#pragma endregion

#pragma region Drawing
void ButterflyDemo::DrawBox()
{
	XMFLOAT4X4 worldMtx;
	XMStoreFloat4x4(&worldMtx, XMMatrixIdentity());
	UpdateBuffer(m_cbWorld, worldMtx);
	m_box.Render(m_device.context());
}

void ButterflyDemo::DrawDodecahedron(bool colors)
//Draw dodecahedron. If color is true, use render faces with corresponding colors. Otherwise render using white color
{
	UpdateBuffer(m_cbSurfaceColor, XMFLOAT4(1, 1, 1, 1));
	//TODO : 1.02. Draw all dodecahedron sides with colors - ignore function parameter for now
	XMFLOAT4X4 worldMtx;
	for (int i = 0; i < 12; i++) {
		XMStoreFloat4x4(&worldMtx, DirectX::XMLoadFloat4x4(&m_dodecahedronMtx[i]));
		UpdateBuffer(m_cbWorld, worldMtx);
		if (colors) {
			UpdateBuffer(m_cbSurfaceColor, COLORS[i]);
		}
		m_pentagon.Render(m_device.context());
	}

	//TODO : 1.14. Modify function so if colors parameter is set to false, all faces are drawn white instead
	
}

void ButterflyDemo::DrawMoebiusStrip()
//TODO : 1.08. Draw the Moebius strip mesh
{
	XMFLOAT4X4 worldMtx;
	XMStoreFloat4x4(&worldMtx, XMMatrixIdentity());
	UpdateBuffer(m_cbWorld, worldMtx);
	//UpdateBuffer(m_cbSurfaceColor, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));
	m_moebius.Render(m_device.context());
}

void ButterflyDemo::DrawButterfly()
//TODO : 1.11. Draw the butterfly
{
	//UpdateBuffer(m_cbSurfaceColor, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
	UpdateBuffer(m_cbWorld, m_wingMtx[0]);
	m_wing.Render(m_device.context());
	UpdateBuffer(m_cbWorld, m_wingMtx[1]);
	m_wing.Render(m_device.context());
}

void ButterflyDemo::DrawBillboards()
//Setup billboards rendering and draw them
{
	//TODO : 1.33. Setup shaders and blend state
	SetBillboardShaders();
	m_device.context()->OMSetBlendState(m_bsAdd.get(), nullptr, BS_MASK);

	//TODO : 1.34. Draw both billboards with appropriate colors and transformations
	XMFLOAT4 pos = m_camera.getCameraPosition();
	XMMATRIX R = XMMatrixInverse(nullptr, m_camera.getViewMatrix()) * XMMatrixTranslation(-pos.x, -pos.y, -pos.z);
	XMMATRIX S = XMMatrixScaling(0.5, 0.5, 0.5);
	XMFLOAT4X4 worldMtx;
	//green
	XMMATRIX T = XMMatrixTranslation(GREEN_LIGHT_POS.x, GREEN_LIGHT_POS.y, GREEN_LIGHT_POS.z);
	UpdateBuffer(m_cbSurfaceColor, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
	XMStoreFloat4x4(&worldMtx, R * S * T);
	UpdateBuffer(m_cbWorld, worldMtx);
	m_bilboard.Render(m_device.context());
	//nlue
	T = XMMatrixTranslation(BLUE_LIGHT_POS.x, BLUE_LIGHT_POS.y, BLUE_LIGHT_POS.z);
	UpdateBuffer(m_cbSurfaceColor, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));
	XMStoreFloat4x4(&worldMtx, R * S * T);
	UpdateBuffer(m_cbWorld, worldMtx);
	m_bilboard.Render(m_device.context());


	//TODO : 1.35. Restore rendering state to it's original values
	m_device.context()->OMSetBlendState(nullptr, nullptr, BS_MASK);
	SetShaders();
}

void ButterflyDemo::DrawMirroredWorld(unsigned int i)
//Draw the mirrored scene reflected in the i-th dodecahedron face
{
	//TODO : 1.22. Setup render state for writing to the stencil buffer
	m_device.context()->OMSetDepthStencilState(m_dssStencilWrite.get(), i + 1);

	//TODO : 1.23. Draw the i-th face
	XMFLOAT4X4 worldMtx;
	XMStoreFloat4x4(&worldMtx, DirectX::XMLoadFloat4x4(m_dodecahedronMtx + i));
	UpdateBuffer(m_cbWorld, worldMtx);
	m_pentagon.Render(m_device.context());

	//TODO : 1.24. Setup depth stencil state for rendering mirrored world
	m_device.context()->OMSetDepthStencilState(m_dssStencilTest.get(), i + 1);

	//TODO : 1.15. Setup rasterizer state and view matrix for rendering the mirrored world
	m_device.context()->RSSetState(m_rsCCW.get());

	//TODO : 1.16. Draw 3D objects of the mirrored scene - dodecahedron should be drawn with only one light and no colors and without blending
	XMFLOAT4X4 view;
	XMMATRIX M = XMLoadFloat4x4(m_mirrorMtx + i) * m_camera.getViewMatrix();
	XMStoreFloat4x4(&view, M);
	UpdateCameraCB(view);
	DrawDodecahedron(false);
	DrawMoebiusStrip();
	DrawButterfly();

	//TODO : 1.17. Restore rasterizer state to it's original value
	m_device.context()->RSSetState(nullptr);

	//TODO : 1.37. Setup depth stencil state for rendering mirrored billboards
	m_device.context()->OMSetDepthStencilState(m_dssStencilTestNoDepthWrite.get(), i + 1);
	//TODO : 1.38. Draw mirrored billboards - they need to be drawn after restoring rasterizer state, but with mirrored view matrix
	DrawBillboards();
	//TODO : 1.18. Restore view matrix to its original value
	XMStoreFloat4x4(&view, m_camera.getViewMatrix());
	UpdateCameraCB(view);

	//TODO : 1.25. Restore depth stencil state to it's original value
	m_device.context()->OMSetDepthStencilState(nullptr, 0);
}

void ButterflyDemo::Render()
{
	Base::Render();

	//render mirrored worlds
	for (int i = 0; i < 12; ++i)
		DrawMirroredWorld(i);

	//render dodecahedron with one light and alpha blending
	m_device.context()->OMSetBlendState(m_bsAlpha.get(), nullptr, BS_MASK);
	Set1Light();
	//TODO : 1.19. Comment the following line for now
	DrawDodecahedron(true);
	//TODO : 1.27. Uncomment the above line again
	m_device.context()->OMSetBlendState(nullptr, nullptr, BS_MASK);

	//render the rest of the scene with all lights
	Set3Lights();
	//TODO : 1.03. [optional] Comment the following line
	//DrawBox();
	DrawMoebiusStrip();
	DrawButterfly();
	m_device.context()->OMSetDepthStencilState(m_dssNoDepthWrite.get(), 0);
	DrawBillboards();
	m_device.context()->OMSetDepthStencilState(nullptr, 0);
}
#pragma endregion