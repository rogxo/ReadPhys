#include <ntifs.h>
#include "Anti4heatExpert.h"


ULONG ReadVirtualMemory(ULONG64 DirectoryTableBase, PVOID Address, PVOID Buffer, PULONG pSizeRead)
{
	BOOLEAN IsDoneRead;
	ULONG ErrorCode;
	ULONG SizeLeft;
	ULONG OffsetInPage;
	ULONG SizeRead;
	ULONG SizeLeftInPage;
	ULONG Size;
	ULONG64 PhysicalAddress;
	ULONG64 PageSize;
	ULONG64 PageAddress;
	Anti4heatExpert::PHYSICAL_PAGE_INFO TransferPageInfo;

	SizeRead = 0;
	memset(&TransferPageInfo, 0, sizeof(TransferPageInfo));
	if (!DirectoryTableBase)
	{
		DirectoryTableBase = __readcr3();
	}
	if (Address && Buffer && pSizeRead && *pSizeRead)
	{
		if (KeGetCurrentIrql() <= 2u)
		{
			Anti4heatExpert::AllocatePhysicalPage(&TransferPageInfo, 0x1000);
			PageSize = 0;
			ErrorCode = Anti4heatExpert::GetPhysPageSize(&TransferPageInfo, (ULONG64)Address, &PageSize, DirectoryTableBase);
			if (!ErrorCode && PageSize > 0x1000)
			{
				ErrorCode = 1;
				goto _EXIT;
			}
			OffsetInPage = (ULONG64)Address & 0xFFF;
			PageAddress = (ULONG64)Address & 0xFFFFFFFFFFFFF000u;
			Size = *pSizeRead;
			SizeLeft = *pSizeRead;
			do
			{
				IsDoneRead = FALSE;
				if (SizeLeft >= PAGE_SIZE - OffsetInPage)
					SizeLeftInPage = PAGE_SIZE - OffsetInPage;
				else
					SizeLeftInPage = SizeLeft;

				PhysicalAddress = 0;
				ErrorCode = Anti4heatExpert::GetPhysPageAddress(&TransferPageInfo, DirectoryTableBase, (PVOID)PageAddress, &PhysicalAddress);

				if (!ErrorCode && PhysicalAddress
					&& !Anti4heatExpert::ReadPhysicalPage(
						&TransferPageInfo,
						PhysicalAddress + OffsetInPage,
						Buffer,
						SizeLeftInPage))
				{
					SizeRead += SizeLeftInPage;
					IsDoneRead = TRUE;
				}
				if (!IsDoneRead)
					memset(Buffer, 0, SizeLeftInPage);
				Buffer = (PUCHAR)Buffer + SizeLeftInPage;
				PageAddress += OffsetInPage + (ULONG64)SizeLeftInPage;
				OffsetInPage = 0;
				SizeLeft -= SizeLeftInPage;
			} while (SizeLeft && SizeLeft < Size);
			if (SizeRead)
			{
				*pSizeRead = SizeRead;
				ErrorCode = 0;
			}
			else
			{
				ErrorCode = 216;
			}
		}
		else
		{
			ErrorCode = 261;
		}
	}
	else
	{
		ErrorCode = 22;
	}
	_EXIT:
	FreePhysicalPage(&TransferPageInfo);
	return ErrorCode;
}


EXTERN_C NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = [](PDRIVER_OBJECT DriverObject)->VOID {
		UNREFERENCED_PARAMETER(DriverObject);
	};

	ULONG Size = 0x1000;
	PVOID VirtualAddress = ExAllocatePool(PagedPool, Size);
	PVOID Buffer = ExAllocatePool(PagedPool, Size);
	if (!VirtualAddress || !Buffer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	memset(VirtualAddress, 'A', Size);
	memset(Buffer, 0, Size);

	ULONG ErrorCode = ReadVirtualMemory(0, VirtualAddress, Buffer, &Size);
	if (!ErrorCode)
	{
		DbgPrint("[+] Read Success!!!\n");

		for (ULONG i = 0; i < 0x10; i++)
		{
			DbgPrint("%c", *(PCHAR)Buffer);
		}
		DbgPrint("\n");
	}
	else
	{

	}

	ExFreePool(VirtualAddress);
	ExFreePool(Buffer);

	return STATUS_SUCCESS;
}

