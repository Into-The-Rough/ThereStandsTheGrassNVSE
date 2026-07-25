#pragma once

//every engine address, vtable slot and struct offset this plugin depends on, asserted once here
//and included by the features that consume them

namespace Engine
{
	constexpr UInt32 kAddr_Renderer = 0x11F9508;
	constexpr UInt32 kRenderer_D3DDevice = 0x288;			//NiDX9Renderer::m_pkD3DDevice9
	constexpr UInt32 kNiD3DVertexShader_Handle = 0x34;		//SetShaderHandle 0xBE0BB0 is just *(this+52) = handle
	constexpr UInt32 kAddr_TES = 0x11DEA10;
	constexpr UInt32 kAddr_LookupForm = 0x4839C0;			//LookupGlobalForm, cdecl refID -> TESForm*

	//slot 24 is the per geometry setup on every NiD3DShader subclass. proven against the grass
	//vtable whose slot 24 is sub_BAAC10. leaf overrides it, frond inherits the shared base, so
	//every install records the original from the slot rather than assuming a function
	constexpr UInt32 kShaderSetupSlot = 24 * 4;
	constexpr UInt32 kVtbl_TallGrassShader = 0x10B8980;
	constexpr UInt32 kVtbl_SpeedTreeLeafShader = 0x10B9190;
	constexpr UInt32 kVtbl_SpeedTreeFrondShader = 0x10BBEB8;

	//NiTransform sits at 0x34, rotate first, stored column major so element(r,c) = flat[c*3+r].
	//traced out of NiMatrix33::FromAxisAngle, which stores R00,R10,R20,R01...
	constexpr UInt32 kNiAVObject_LocalTransform = 0x34;
	constexpr UInt32 kNiAVObject_UpdateTransformAndBounds = 0xC0 / 4;

	constexpr UInt32 kCell_ObjectList = 0xAC;
	constexpr UInt32 kRefr_RenderState = 0x64;
	constexpr UInt32 kRenderState_RootNode = 0x14;

	//com and d3d9 vtable indices, abi frozen. 91 verified against the engine call at 0xBE0FE0
	enum
	{
		kCom_Release = 2,
		kD3DVS_GetFunction = 4,
		kD3DDev_CreateVertexShader = 91,
		kD3DDev_SetVertexShaderConstantF = 94,
	};

	//vertex shader constant registers this plugin owns. grass is vs_3_0 so anything is legal and
	//the grass set never reaches past c23. frond shaders are vs_1_1 and can only read c0-c95, and
	//the tree set never reaches past c85, so the tree pair has to live low
	constexpr UInt32 kConst_Grass = 248;
	constexpr UInt32 kConst_Tree = 90;

	//minimal views of the few game types this plugin touches. the full sdk game headers are not
	//pulled in for six fields, per the project rule on header chains
	struct FormView
	{
		void	*vtbl;
		UInt8	typeID;			// 04
		UInt8	typeIDPad[3];
		UInt32	flags;			// 08
		UInt32	refID;			// 0C
	};
	static_assert(offsetof(FormView, refID) == 0x0C);

	struct RefrView
	{
		UInt8		pad00[0x0C];
		UInt32		refID;			// 0C
		UInt8		pad10[0x10];
		FormView	*baseForm;		// 20
		UInt8		pad24[0x0C];
		float		position[3];	// 30
		UInt8		pad3C[0x04];
		void		*parentCell;	// 40
		UInt8		pad44[0x20];
		UInt8		*renderState;	// 64
	};
	static_assert(offsetof(RefrView, baseForm) == 0x20);
	static_assert(offsetof(RefrView, position) == 0x30);
	static_assert(offsetof(RefrView, parentCell) == 0x40);
	static_assert(offsetof(RefrView, renderState) == 0x64);

	struct CellRefNode
	{
		RefrView	*ref;
		CellRefNode	*next;
	};

	struct CellView
	{
		UInt8		pad00[0xAC];
		CellRefNode	objectList;		// AC
		UInt8		padB4[0x0C];
		void		*worldSpace;	// C0, null means interior
	};
	static_assert(offsetof(CellView, objectList) == 0xAC);
	static_assert(offsetof(CellView, worldSpace) == 0xC0);

	constexpr UInt32 kAddr_Player = 0x11DEA3C;
	inline RefrView* Player() { return *(RefrView**)kAddr_Player; }

	struct GridCellArrayView
	{
		UInt8			pad00[0x0C];
		UInt32			gridSize;		// 0C
		CellView		**cells;		// 10
	};
	static_assert(offsetof(GridCellArrayView, gridSize) == 0x0C);
	static_assert(offsetof(GridCellArrayView, cells) == 0x10);

	struct TESView
	{
		UInt8				pad00[8];
		GridCellArrayView	*grid;		// 08
	};
	static_assert(offsetof(TESView, grid) == 0x08);

	inline void* GetD3DDevice()
	{
		UInt8 *renderer = *(UInt8**)kAddr_Renderer;
		return renderer ? *(void**)(renderer + kRenderer_D3DDevice) : nullptr;
	}

	inline void** ComVtbl(void *obj) { return *(void***)obj; }

	inline void ComRelease(void *obj)
	{
		((ULONG(__stdcall*)(void*))ComVtbl(obj)[kCom_Release])(obj);
	}

	inline void SetVertexShaderConstants(void *device, UInt32 startReg, const float *values, UInt32 vec4Count)
	{
		((HRESULT(__stdcall*)(void*, UInt32, const float*, UInt32))
			ComVtbl(device)[kD3DDev_SetVertexShaderConstantF])(device, startReg, values, vec4Count);
	}

	inline RefrView* LookupRef(UInt32 refID)
	{
		return ((RefrView*(__cdecl*)(UInt32))kAddr_LookupForm)(refID);
	}

	inline void* RefRootNode(RefrView *ref)
	{
		UInt8 *renderState = *(UInt8**)((UInt8*)ref + kRefr_RenderState);
		return renderState ? *(void**)(renderState + kRenderState_RootNode) : nullptr;
	}
}
