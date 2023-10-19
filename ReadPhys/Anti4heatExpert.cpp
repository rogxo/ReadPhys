#include "Anti4heatExpert.h"


namespace Anti4heatExpert
{
	ULONG64 g_PteBase = 0;
	ULONG64 g_PdeBase = 0;
	ULONG64 g_PpeBase = 0;
	ULONG64 g_PxeBase = 0;
	BOOLEAN g_IsInitPteBaseForSystem = false;
	PPHYSICAL_MEMORY_RANGE g_PhysicalMemoryRanges = 0;


	KIRQL __fastcall RaiseIRQL(KIRQL NewIrql)
	{
		KIRQL CurrentIrql;

		CurrentIrql = KeGetCurrentIrql();
		__writecr8(NewIrql);
		return CurrentIrql;
	}

	KIRQL __fastcall RaiseIrqlToDpcLv()
	{
		return RaiseIRQL(2u);
	}

	void __fastcall LowerIrql(KIRQL Irql)
	{
		__writecr8(Irql);
	}

	bool __fastcall IsPhysPageInRange(ULONG64 PhysPageBase, ULONG64 Size)
	{
		bool Flag;
		int i;
		PHYSICAL_MEMORY_RANGE PhysicalMemoryRnage;
		ULONG64 PhysPageEnd;

		Flag = 0;
		if (!g_PhysicalMemoryRanges)
		{
			if (KeGetCurrentIrql())
				return Flag;
			g_PhysicalMemoryRanges = MmGetPhysicalMemoryRanges();
		}
		if (!g_PhysicalMemoryRanges)
			return Flag;
		PhysPageEnd = PhysPageBase + Size - 1;
		for (i = 0; ; ++i)
		{
			PhysicalMemoryRnage = g_PhysicalMemoryRanges[i];
			if (!PhysicalMemoryRnage.BaseAddress.QuadPart || !PhysicalMemoryRnage.NumberOfBytes.QuadPart)
				break;
			if (PhysPageBase >= (ULONG64)PhysicalMemoryRnage.BaseAddress.QuadPart
				&& PhysPageBase < (ULONG64)PhysicalMemoryRnage.NumberOfBytes.QuadPart + PhysicalMemoryRnage.BaseAddress.QuadPart
				&& PhysPageEnd >= (ULONG64)PhysicalMemoryRnage.BaseAddress.QuadPart
				&& PhysPageEnd < (ULONG64)PhysicalMemoryRnage.NumberOfBytes.QuadPart + PhysicalMemoryRnage.BaseAddress.QuadPart)
			{
				return 1;
			}
		}
		return Flag;
	}

	bool __fastcall IsVaPhysicalAddressValid(PVOID VirtualAddress)
	{
		return MmGetPhysicalAddress(VirtualAddress).QuadPart > 0x1000;
	}

	PVOID __fastcall GetPML4Base(PHYSICAL_ADDRESS DirectoryTableBase)
	{
		PVOID VirtualForPhysical;

		VirtualForPhysical = MmGetVirtualForPhysical(DirectoryTableBase);
		if ((ULONG64)VirtualForPhysical <= 0x1000)
			return 0;
		else
			return VirtualForPhysical;
	}

	ULONG __fastcall InitializePteBase()
	{
		ULONG64 Cr3;
		ULONG ErrorCode;
		ULONG64 index;
		PULONG64 PML4Table;
		PHYSICAL_ADDRESS DirectoryTableBase;
		ULONG64 Item;

		ErrorCode = 1;
		if (!g_IsInitPteBaseForSystem)
		{
			Cr3 = __readcr3();
			DirectoryTableBase.QuadPart = ((Cr3 >> 12) & 0xFFFFFFFFFFi64) << 12;// 去除控制位，拿到DirectoryTableBase
			PML4Table = (PULONG64)GetPML4Base(DirectoryTableBase);
			if (PML4Table)
			{
				for (index = 0; index < 0x200; ++index)
				{
					Item = PML4Table[index];
					if (((Item >> 12) & 0xFFFFFFFFFFi64) == ((Cr3 >> 12) & 0xFFFFFFFFFFi64))
					{
						g_PteBase = (index << 39) - 0x1000000000000;// + 0xFFFF000000000000
						g_PdeBase = (index << 30) + (index << 39) - 0x1000000000000;
						g_PpeBase = (index << 21) + g_PdeBase;
						g_PxeBase = (index << 12) + (index << 21) + g_PdeBase;
						ErrorCode = 0;
						break;
					}
					g_IsInitPteBaseForSystem = TRUE;
				}
			}
			else
			{
				ErrorCode = 0x106;
			}
		}
		else
		{
			ErrorCode = 0;
		}
		return ErrorCode;
	}

	ULONG64 __fastcall GetPteAddress(PVOID VirtualAddress)
	{
		return g_PteBase + 8 * (((ULONG64)VirtualAddress & 0xFFFFFFFFFFFFi64) >> 12);
	}

	ULONG __fastcall AllocatePhysicalPage(
		PHYSICAL_PAGE_INFO* PhysicalPageInfo,
		SIZE_T Size)
	{
		ULONG ErrorCode;
		PVOID BaseAddress;
		PVOID PteAddress;

		if (PhysicalPageInfo && Size)
		{
			if (Size == 0x1000)
			{
				ErrorCode = InitializePteBase();
				if (!ErrorCode)
				{
					BaseAddress = MmAllocateMappingAddress(0x1000, 'axe');
					if (BaseAddress)
					{
						PteAddress = (PVOID)GetPteAddress(BaseAddress);
						if (PteAddress && IsVaPhysicalAddressValid(PteAddress))
						{
							PhysicalPageInfo->BaseAddress = BaseAddress;
							PhysicalPageInfo->Size = 0x1000;
							PhysicalPageInfo->PteAddress = PteAddress;
							ErrorCode = 0;
						}
						else
						{
							MmFreeMappingAddress(BaseAddress, 'axe');
							ErrorCode = 0x109;
						}
					}
					else
					{
						ErrorCode = 0x119;
					}
				}
			}
			else
			{
				ErrorCode = 50;
			}
		}
		else
		{
			ErrorCode = 22;
		}
		if (ErrorCode && PhysicalPageInfo)
			memset(PhysicalPageInfo, 0i64, sizeof(PHYSICAL_PAGE_INFO));
		return ErrorCode;
	}

	void __fastcall FreePhysicalPage(
		PHYSICAL_PAGE_INFO* PageInfo)
	{
		if (PageInfo)
		{
			if (PageInfo->BaseAddress)
			{
				MmFreeMappingAddress(PageInfo->BaseAddress, 'axe');
				memset(PageInfo, 0i64, sizeof(PHYSICAL_PAGE_INFO));
			}
		}
	}

	ULONG __fastcall ReadPhysicalPage(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 PhysPageBase,
		PVOID Buffer,
		SIZE_T Size)
	{
		bool IsDpcLevel;
		unsigned __int8 OldIrql;
		ULONG ErrorCode;
		PVOID PteAddress;
		ULONG64 OldPte;

		IsDpcLevel = 0;
		OldIrql = 0;
		if (PhysPageBase && Buffer && Size)
		{
			if (TransferPageInfo && TransferPageInfo->BaseAddress && TransferPageInfo->PteAddress)
			{
				if (Size <= TransferPageInfo->Size)
				{
					if (PhysPageBase >> 12 == (PhysPageBase + Size - 1) >> 12)	// 判断是否在一页内
					{
						if (IsPhysPageInRange(PhysPageBase, Size))	// 判断物理页是否在合法范围
						{
							if (KeGetCurrentIrql() < 2u)
							{
								OldIrql = RaiseIrqlToDpcLv();
								IsDpcLevel = 1;
							}
							if (IsVaPhysicalAddressValid(TransferPageInfo->PteAddress))
							{
								PteAddress = TransferPageInfo->PteAddress;
								OldPte = *(ULONG64*)PteAddress;   // 保存原始页面PTE
								*(ULONG64*)PteAddress = (((PhysPageBase >> 12) & 0xFFFFFFFFFFi64) << 12) | *(ULONG64*)PteAddress & 0xFFF0000000000EF8 | 0x103;
								__invlpg(TransferPageInfo->BaseAddress);	// 挂物理页并刷新TLB来读取物理地址
								memmove(Buffer, (char*)TransferPageInfo->BaseAddress + (PhysPageBase & 0xFFF), Size);
								*(ULONG64*)PteAddress = OldPte;
								ErrorCode = 0;
							}
							else
							{
								ErrorCode = 265;
							}
						}
						else
						{
							ErrorCode = 276;
						}
					}
					else
					{
						ErrorCode = 275;
					}
				}
				else
				{
					ErrorCode = 279;
				}
			}
			else
			{
				ErrorCode = 157;
			}
		}
		else
		{
			ErrorCode = 22;
		}
		if (IsDpcLevel)
			LowerIrql(OldIrql);
		return ErrorCode;
	}

	ULONG __fastcall GetPageTableInfo(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 Cr3,
		ULONG64 PageAddress,
		PAGE_TABLE_INFO* PageTableInfo)
	{
		if (!PageTableInfo)
			return 22;
		memset(PageTableInfo, 0i64, sizeof(PAGE_TABLE_INFO));
		if (ReadPhysicalPage(
			TransferPageInfo,
			(((Cr3 >> 12) & 0xFFFFFFFFFFi64) << 12) + 8 * ((PageAddress >> 39) & 0x1FF),
			PageTableInfo,
			8)
			|| (PageTableInfo->Pxe & 1) == 0)
		{
			return 262;
		}
		if (((PageTableInfo->Pxe >> 12) & 0xFFFFFFFFFFi64) == ((Cr3 >> 12) & 0xFFFFFFFFFFi64))
			return 266;
		if (ReadPhysicalPage(
			TransferPageInfo,
			(((PageTableInfo->Pxe >> 12) & 0xFFFFFFFFFFi64) << 12) + 8 * ((PageAddress >> 30) & 0x1FF),
			&PageTableInfo->Ppe,
			8)
			|| (PageTableInfo->Ppe & 1) == 0)
		{
			return 263;
		}
		if (((PageTableInfo->Ppe >> 7) & 1) != 0)
		{
			PageTableInfo->PageType = 7;                // 1GB large page
			return 0;
		}
		else if (!ReadPhysicalPage(
			TransferPageInfo,
			(((PageTableInfo->Ppe >> 12) & 0xFFFFFFFFFFi64) << 12) + 8 * ((PageAddress >> 21) & 0x1FF),
			&PageTableInfo->Pde,
			8)
			&& (PageTableInfo->Pde & 1) != 0)
		{
			if (((PageTableInfo->Pde >> 7) & 1) != 0)
			{
				PageTableInfo->PageType = 6;              // 2MB large page
				return 0;
			}
			else if (!ReadPhysicalPage(
				TransferPageInfo,
				(((PageTableInfo->Pde >> 12) & 0xFFFFFFFFFFi64) << 12) + 8 * ((PageAddress >> 12) & 0x1FF),
				&PageTableInfo->Pte,
				8)
				&& (PageTableInfo->Pte & 1) != 0)
			{
				PageTableInfo->PageType = 5;              // 4KB Page
				return 0;
			}
			else
			{
				return 265;
			}
		}
		else
		{
			return 264;
		}
	}

	ULONG __fastcall GetPhysPageAddress(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 TargetCr3,
		PVOID PageVa,
		PULONG64 pPhysicalAddress)
	{
		ULONG64 CurrentCr3;
		ULONG ErrorCode;
		ULONG64 PagePhys;
		SIZE_T PageSize;
		PAGE_TABLE_INFO PageTableInfo;
		ULONG64 Cr3;

		Cr3 = TargetCr3;
		if (!pPhysicalAddress)
			return 22;
		*pPhysicalAddress = 0;
		memset(&PageTableInfo, 0, 36);
		if (!TargetCr3)
		{
			CurrentCr3 = __readcr3();
			Cr3 = CurrentCr3;
		}
		ErrorCode = GetPageTableInfo(TransferPageInfo, Cr3, (ULONG64)PageVa, &PageTableInfo);// 计算页物理地址
		if (ErrorCode)
			return ErrorCode;
		PagePhys = 0;
		PageSize = 0;
		switch (PageTableInfo.PageType)
		{
		case 5u:
			PagePhys = ((PageTableInfo.Pte >> 12) & 0xFFFFFFFFFFi64) << 12;
			PageSize = 0x1000;
			break;
		case 6u:
			PagePhys = ((PageTableInfo.Pde >> 21) & 0x7FFFFFFF) << 21;
			PageSize = 0x200000;
			break;
		case 7u:
			PagePhys = ((PageTableInfo.Ppe >> 30) & 0x3FFFFF) << 30;
			PageSize = 0x40000000;
			break;
		}
		if (PagePhys)
		{
			*pPhysicalAddress = (ULONG64)PageVa + PagePhys - (~(PageSize - 1) & (ULONG64)PageVa);
			return 0;
		}
		else
		{
			return 276;
		}
	}

	ULONG __fastcall GetPhysPageSize(
		PHYSICAL_PAGE_INFO* TransferPageInfo,
		ULONG64 PageAddress,
		PULONG64 pPageSize,
		ULONG64 TargetCr3)
	{
		ULONG ErrorCode;
		ULONG Flag;
		ULONG v8;
		ULONG v9;
		SIZE_T PageSize;
		PAGE_TABLE_INFO PageTableInfo;
		ULONG64 Cr3;

		Cr3 = TargetCr3;
		ErrorCode = 0;
		memset(&PageTableInfo, 0, 0x24);
		if (!TargetCr3)
		{
			Cr3 = __readcr3();
		}
		if (GetPageTableInfo(TransferPageInfo, Cr3, PageAddress, &PageTableInfo))
		{
			ErrorCode = 2;
			if ((PageTableInfo.Pxe & 1) != 0)
			{
				if ((PageTableInfo.Ppe & 1) != 0 && ((PageTableInfo.Ppe >> 7) & 1) == 0)
				{
					if ((PageTableInfo.Pde & 1) != 0 && ((PageTableInfo.Pde >> 7) & 1) == 0)
						PageSize = 0x1000;
					else
						PageSize = 0x200000;
				}
				else
				{
					PageSize = 0x40000000;
				}
			}
			else
			{
				PageSize = 0x8000000000;
			}
		}
		else
		{
			switch (PageTableInfo.PageType)
			{
			case 5u:
				PageSize = 0x1000;
				Flag = 16;
				if ((PageTableInfo.Ppe & 1) != 0)
				{
					if ((PageTableInfo.Ppe & 0x8000000000000000) != 0i64)
						Flag = 24;
					if (((PageTableInfo.Ppe >> 1) & 1) == 0)
						Flag |= 0x80u;
					if (((PageTableInfo.Ppe >> 2) & 1) == 0)
						Flag |= 4u;
					if ((PageTableInfo.Pde & 1) != 0)
					{
						if ((PageTableInfo.Pde & 0x8000000000000000) != 0i64)
							Flag |= 8u;
						if (((PageTableInfo.Pde >> 1) & 1) == 0)
							Flag |= 0x80u;
						if (((PageTableInfo.Pde >> 2) & 1) == 0)
							Flag |= 4u;
						if ((PageTableInfo.Pte & 1) != 0)
						{
							if ((PageTableInfo.Pte & 0x8000000000000000) != 0i64)
								Flag |= 8u;
							if (((PageTableInfo.Pte >> 1) & 1) == 0)
								Flag |= 0x80u;
							if (((PageTableInfo.Pte >> 2) & 1) == 0)
								Flag |= 4u;
							if (((PageTableInfo.Pte >> 5) & 1) != 0)
								Flag |= 0x100u;
							if (((PageTableInfo.Pte >> 6) & 1) != 0)
								Flag |= 0x200u;
							ErrorCode = Flag | 1;
						}
						else
						{
							ErrorCode = Flag | 2;
						}
					}
					else
					{
						ErrorCode = Flag | 2;
					}
				}
				else
				{
					ErrorCode = 18;
				}
				break;
			case 6u:
				PageSize = 0x200000;
				v8 = 32;
				if ((PageTableInfo.Ppe & 1) != 0)
				{
					if ((PageTableInfo.Ppe & 0x8000000000000000) != 0i64)
						v8 = 40;
					if (((PageTableInfo.Ppe >> 1) & 1) == 0)
						v8 |= 0x80u;
					if (((PageTableInfo.Ppe >> 2) & 1) == 0)
						v8 |= 4u;
					if ((PageTableInfo.Pde & 1) != 0)
					{
						if ((PageTableInfo.Pde & 0x8000000000000000) != 0i64)
							v8 |= 8u;
						if (((PageTableInfo.Pde >> 1) & 1) == 0)
							v8 |= 0x80u;
						if (((PageTableInfo.Pde >> 2) & 1) == 0)
							v8 |= 4u;
						if (((PageTableInfo.Pde >> 5) & 1) != 0)
							v8 |= 0x100u;
						if (((PageTableInfo.Pde >> 6) & 1) != 0)
							v8 |= 0x200u;
						ErrorCode = v8 | 1;
					}
					else
					{
						ErrorCode = v8 | 2;
					}
				}
				else
				{
					ErrorCode = 34;
				}
				break;
			case 7u:
				PageSize = 0x40000000;
				v9 = 64;
				if ((PageTableInfo.Ppe & 1) != 0)
				{
					if ((PageTableInfo.Ppe & 0x8000000000000000) != 0i64)
						v9 = 72;
					if (((PageTableInfo.Ppe >> 1) & 1) == 0)
						v9 |= 0x80u;
					if (((PageTableInfo.Ppe >> 2) & 1) == 0)
						v9 |= 4u;
					if (((PageTableInfo.Ppe >> 5) & 1) != 0)
						v9 |= 0x100u;
					if (((PageTableInfo.Ppe >> 6) & 1) != 0)
						v9 |= 0x200u;
					ErrorCode = v9 | 1;
				}
				else
				{
					ErrorCode = 66;
				}
				break;
			default:
				PageSize = 0x1000;
				break;
			}
		}
		if (pPageSize)
			*pPageSize = PageSize;
		return ErrorCode;
	}

}
