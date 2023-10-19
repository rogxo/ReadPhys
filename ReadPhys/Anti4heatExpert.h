#include <ntifs.h>
#include <intrin.h>


namespace Anti4heatExpert
{
	struct PHYSICAL_PAGE_INFO
	{
		PVOID BaseAddress;
		SIZE_T Size;
		PVOID PteAddress;
	};

	struct PAGE_TABLE_INFO
	{
		ULONG64 Pxe;
		ULONG64 Ppe;
		ULONG64 Pde;
		ULONG64 Pte;
		ULONG PageType;
	};
}

namespace Anti4heatExpert
{
	ULONG __fastcall AllocatePhysicalPage(
		PHYSICAL_PAGE_INFO* PhysicalPageInfo,
		SIZE_T Size);

	void __fastcall FreePhysicalPage(
		PHYSICAL_PAGE_INFO* PageInfo);

	ULONG __fastcall ReadPhysicalPage(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 PhysPageBase,
		PVOID Buffer,
		SIZE_T Size);

	ULONG __fastcall GetPageTableInfo(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 Cr3,
		ULONG64 PageAddress,
		PAGE_TABLE_INFO* PageTableInfo);

	ULONG __fastcall GetPhysPageAddress(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 TargetCr3,
		PVOID PageVa,
		PULONG64 pPhysicalAddress);

	ULONG __fastcall GetPhysPageSize(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 PageAddress,
		PULONG64 pPageSize,
		ULONG64 TargetCr3);
}