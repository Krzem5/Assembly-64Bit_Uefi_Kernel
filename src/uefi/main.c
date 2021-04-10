#include <cpu/idt.h>
#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <kmain.h>
#include <memory/paging.h>
#include <stdint.h>



#ifdef __DEBUG_BUILD__
#define UEFI_LOADER_LOG(...) Print(__VA_ARGS__)
#else
#define UEFI_LOADER_LOG(...)
#endif
#define MIN_MEM_ADDRESS 0x100000
#define IDT_SIZE (IDT_INTERRUPT_COUNT*sizeof(idt_entry_t))
#define KERNEL_STACK_SIZE (PAGE_4KB_SIZE*16)
#define KERNEL_DATA_PAGE_COUNT ((IDT_SIZE+KERNEL_STACK_SIZE+PAGE_4KB_SIZE-1)>>PAGE_4KB_POWER_OF_2)
#define MAX_KERNEL_FILE_SIZE 0x1000000
#define PML4_PHYSICAL_ADDRESS 0x200000
#define ELF_HEADER_MAGIC 0x464c457f
#define ELF_HEADER_WORD_SIZE 2
#define ELF_HEADER_ENDIANESS 1
#define ELF_HEADER_VERSION 1
#define ELF_HEADER_OS_ABI 0
#define ELF_HEADER_TAREGT_MACHINE 0x3e
#define ELF_PT_LOAD 0x01
#define ACPI_20_REVISION 2
#define ACPI_XSDT_APIC_TYPE 0x43495041
#define ACPI_XSDT_FACP_TYPE 0x50434146
#define ACPI_XSDT_HPET_TYPE 0x54455048



extern uint64_t* KERNEL_CALL asm_clear_pages_get_cr3(uint64_t pml4,uint64_t pg_c);
extern void KERNEL_CALL asm_enable_paging(uint64_t pml4);
extern void KERNEL_CALL asm_halt(void);



struct ELF_HEADER{
	uint32_t m;
	uint8_t ws;
	uint8_t ed;
	uint8_t hv;
	uint8_t abi;
	uint64_t _;
	uint16_t t;
	uint16_t tm;
	uint32_t v;
	uint64_t e;
	uint64_t ph_off;
	uint64_t sh_off;
	uint32_t f;
	uint16_t eh_sz;
	uint16_t pn_sz;
	uint16_t ph_n;
	uint16_t sh_sz;
	uint16_t sh_n;
	uint16_t sh_str_i;
} __attribute__((packed));



struct ELF_PROGRAM_HEADER{
	uint32_t t;
	uint32_t f;
	uint64_t off;
	uint64_t va;
	uint64_t pa;
	uint64_t f_sz;
	uint64_t m_sz;
	uint64_t a;
} __attribute__((packed));



struct ACPI_TABLE_HEADER{
	uint32_t t;
	uint32_t l;
	uint8_t rv;
	uint8_t c;
	char oem_id[6];
	char oem_t[8];
	uint32_t oem_rv;
	uint32_t c_id;
	uint32_t c_rv;
} __attribute__((packed));



struct ACPI_XSDT{
	struct ACPI_TABLE_HEADER h;
	struct ACPI_TABLE_HEADER* hl[];
} __attribute__((packed));



struct ACPI_RSDP{
	char sig[8];
	uint8_t c;
	char oem_id[6];
	uint8_t rv;
	uint32_t rsdt;
	uint32_t l;
	struct ACPI_XSDT* xsdt;
	uint8_t c2;
	uint8_t _[3];
} __attribute__((packed));



static EFI_GUID efi_gop_guid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID efi_acpi2_guid=ACPI_20_TABLE_GUID;
static EFI_GUID efi_lip_guid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID efi_sfs_guid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID efi_fi_guid=EFI_FILE_INFO_ID;
static uint16_t* kernel_fp=L"KERNEL.ELF";
static const char* acpi_rsdp_sig="RSD PTR ";



void efi_main(EFI_HANDLE ih,EFI_SYSTEM_TABLE* st){
	InitializeLib(ih,st);
	EFI_STATUS s=st->BootServices->SetWatchdogTimer(0,0,0,NULL);
	if (EFI_ERROR(s)){
		Print(L"Unable to Disable the Reset Timer!\r\n");
		goto _end;
	}
	EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
	s=st->BootServices->LocateProtocol(&efi_gop_guid,NULL,(void**)&gop);
	if (EFI_ERROR(s)){
		Print(L"Unable to load GOP!\r\n");
		goto _end;
	}
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mi;
	UINT64 mi_sz;
	s=gop->QueryMode(gop,(gop->Mode==NULL?0:gop->Mode->Mode),&mi_sz,&mi);
	if (s==EFI_NOT_STARTED){
		s=gop->SetMode(gop,0);
	}
	if (EFI_ERROR(s)){
		Print(L"Unable to get native mode!\r\n");
		goto _end;
	}
	UINT32 mx=0;
	UINT32 my=0;
	UINT64 mp=0;
	UINT64 m=-1;
	for (UINT64 i=0;i<gop->Mode->MaxMode;i++){
		s=gop->QueryMode(gop,i,&mi_sz,&mi);
		if (EFI_ERROR(s)){
			goto _end;
		}
		if (mi->PixelFormat!=PixelBlueGreenRedReserved8BitPerColor){
			continue;
		}
		UINT64 p=(UINT64)mi->HorizontalResolution*mi->VerticalResolution;
		if (p>mp||(p==mp&&mi->HorizontalResolution>mx)){
			mx=mi->HorizontalResolution;
			my=mi->VerticalResolution;
			mp=p;
			m=i;
		}
	}
	(void)my;
	s=gop->SetMode(gop,m);
	if (EFI_ERROR(s)){
		Print(L"Unable to Set Display Mode: %03d\r\n",m);
		goto _end;
	}
	else{
		UEFI_LOADER_LOG(L"Display Mode: %x -> +%d, %d x %d\r\n",gop->Mode->FrameBufferBase,gop->Mode->FrameBufferSize,gop->Mode->Info->HorizontalResolution,gop->Mode->Info->VerticalResolution);
	}
	UEFI_LOADER_LOG(L"Starting Loader...\r\n");
	struct ACPI_RSDP* acpi=NULL;
	(void)efi_acpi2_guid;
	for (uint64_t i=0;i<st->NumberOfTableEntries;i++){
		if (CompareGuid(&(st->ConfigurationTable+i)->VendorGuid,&efi_acpi2_guid)==0){
			acpi=(struct ACPI_RSDP*)(st->ConfigurationTable+i)->VendorTable;
			break;
		}
	}
	if (acpi==NULL){
		Print(L"No ACPI Table Found!\r\n");
		goto _end;
	}
	UEFI_LOADER_LOG(L"Found ACPI Table: %llx\r\n",acpi);
	for (uint8_t i=0;i<8;i++){
		if (*(acpi->sig+i)!=*(acpi_rsdp_sig+i)){
			Print(L"ACPI RSDP Signature not Matching!\r\n");
			goto _end;
		}
	}
	if (acpi->rv!=ACPI_20_REVISION){
		Print(L"Invalid ACPI Revision (%u)!\r\n",acpi->rv);
		goto _end;
	}
	if (acpi->xsdt==NULL){
		Print(L"ACPI XSDT is Null!\r\n");
		goto _end;
	}
	void* apic=NULL;
	void* fadt=NULL;
	void* hpet=NULL;
	for (uint64_t i=0;i<(acpi->xsdt->h.l-sizeof(struct ACPI_XSDT))/sizeof(struct ACPI_TABLE_HEADER*);i++){
		if (acpi->xsdt->hl[i]->t==ACPI_XSDT_APIC_TYPE){
			apic=acpi->xsdt->hl[i];
		}
		else if (acpi->xsdt->hl[i]->t==ACPI_XSDT_FACP_TYPE){
			fadt=acpi->xsdt->hl[i];
		}
		else if (acpi->xsdt->hl[i]->t==ACPI_XSDT_HPET_TYPE){
			hpet=acpi->xsdt->hl[i];
		}
		else{
			UEFI_LOADER_LOG(L"Unknown XSDT Entry Type: %llx (%c%c%c%c)\r\n",acpi->xsdt->hl[i]->t,acpi->xsdt->hl[i]->t&0xff,(acpi->xsdt->hl[i]->t>>8)&0xff,(acpi->xsdt->hl[i]->t>>16)&0xff,(acpi->xsdt->hl[i]->t>>24)&0xff);
		}
	}
	UEFI_LOADER_LOG(L"APIC = %llx, FADT = %llx, HPET = %llx\r\n",apic,fadt,hpet);
	uint64_t mm_sz=sizeof(EFI_MEMORY_DESCRIPTOR)*32;
	uint64_t mm_k=0;
	uint64_t mm_ds=0;
	uint32_t mm_v=0;
	uint8_t* bf=NULL;
	do{
		bf=AllocatePool(mm_sz);
		if (bf==NULL){
			break;
		}
		s=st->BootServices->GetMemoryMap(&mm_sz,(EFI_MEMORY_DESCRIPTOR*)bf,&mm_k,&mm_ds,&mm_v);
		if (s!=EFI_SUCCESS){
			FreePool(bf);
			mm_sz+=sizeof(EFI_MEMORY_DESCRIPTOR)*32;
		}
	} while (s!=EFI_SUCCESS);
	if (bf==NULL){
		Print(L"Error Reading Memory Map!\r\n");
		goto _end;
	}
	uint64_t sz=0;
	uint8_t lt=0;
	uint64_t le=-1;
	uint8_t* nbf=bf;
	for (uint64_t i=0;i<mm_sz/mm_ds;i++){
		if (((EFI_MEMORY_DESCRIPTOR*)nbf)->PhysicalStart>=MIN_MEM_ADDRESS&&(((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiConventionalMemory||((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiBootServicesCode||((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiBootServicesData||((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiLoaderCode||((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiLoaderData||((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiRuntimeServicesCode||((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiRuntimeServicesData||((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiACPIReclaimMemory)){
			uint8_t t=(((EFI_MEMORY_DESCRIPTOR*)nbf)->Type==EfiACPIReclaimMemory);
			if (lt!=t||le!=((EFI_MEMORY_DESCRIPTOR*)nbf)->PhysicalStart){
				sz++;
				lt=t;
			}
			le=((EFI_MEMORY_DESCRIPTOR*)nbf)->PhysicalStart+((EFI_MEMORY_DESCRIPTOR*)nbf)->NumberOfPages*PAGE_4KB_SIZE;
		}
		nbf+=mm_ds;
	}
	KernelArgs* ka;
	s=st->BootServices->AllocatePages(AllocateAnyPages,0x80000000,(sizeof(KernelArgs)+sizeof(KernelArgsMemEntry)*sz+PAGE_4KB_SIZE-1)>>PAGE_4KB_POWER_OF_2,(EFI_PHYSICAL_ADDRESS*)&ka);
	if (EFI_ERROR(s)){
		Print(L"Unable Allocate Pages for Kernel Args!\r\n");
		goto _end;
	}
	ka->vmem=(uint32_t*)gop->Mode->FrameBufferBase;
	ka->vmem_l=gop->Mode->FrameBufferSize/sizeof(uint32_t);
	ka->vmem_w=gop->Mode->Info->HorizontalResolution;
	ka->vmem_h=gop->Mode->Info->VerticalResolution;
	ka->apic=apic;
	ka->fadt=fadt;
	ka->hpet=hpet;
	ka->mmap_l=sz;
	uint64_t tm=0;
	uint64_t j=0;
	lt=0;
	le=0;
	ka->mmap[0].b=0;
	ka->mmap[0].l=0;
	for (uint64_t i=0;i<mm_sz/mm_ds;i++){
		if (((EFI_MEMORY_DESCRIPTOR*)bf)->PhysicalStart>=MIN_MEM_ADDRESS&&(((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiConventionalMemory||((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiBootServicesCode||((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiBootServicesData||((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiLoaderCode||((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiLoaderData||((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiRuntimeServicesCode||((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiRuntimeServicesData||((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiACPIReclaimMemory)){
			uint8_t t=(((EFI_MEMORY_DESCRIPTOR*)bf)->Type==EfiACPIReclaimMemory);
			if (j==0&&!ka->mmap[0].b&&!ka->mmap[0].l){
				ka->mmap[0].b=((EFI_MEMORY_DESCRIPTOR*)bf)->PhysicalStart|t;
				lt=t;
			}
			else if (lt!=t||le!=((EFI_MEMORY_DESCRIPTOR*)bf)->PhysicalStart){
				Print(L"  %llx - +%llx (%d)\r\n",KERNEL_MEM_MAP_GET_BASE(ka->mmap[j].b),ka->mmap[j].l,KERNEL_MEM_MAP_GET_ACPI(ka->mmap[j].b));
				j++;
				ka->mmap[j].b=((EFI_MEMORY_DESCRIPTOR*)bf)->PhysicalStart|t;
				ka->mmap[j].l=0;
				lt=t;
			}
			ka->mmap[j].l+=((EFI_MEMORY_DESCRIPTOR*)bf)->NumberOfPages*PAGE_4KB_SIZE;
			le=((EFI_MEMORY_DESCRIPTOR*)bf)->PhysicalStart+((EFI_MEMORY_DESCRIPTOR*)bf)->NumberOfPages*PAGE_4KB_SIZE;
			tm+=((EFI_MEMORY_DESCRIPTOR*)bf)->NumberOfPages*PAGE_4KB_SIZE;
		}
		bf+=mm_ds;
	}
	FreePool(bf);
	UEFI_LOADER_LOG(L"  %llx - +%llx (%d)\r\nTotal: %llu (%llu sectors)\r\nAllocating Pages...\r\n",KERNEL_MEM_MAP_GET_BASE(ka->mmap[j].b),ka->mmap[j].l,KERNEL_MEM_MAP_GET_ACPI(ka->mmap[j].b),tm,sz);
	EFI_LOADED_IMAGE_PROTOCOL* lip;
	s=st->BootServices->HandleProtocol(ih,&efi_lip_guid,(void**)&lip);
	if (EFI_ERROR(s)){
		Print(L"Unable to load LIP!\r\n");
		goto _end;
	}
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* sfs;
	s=st->BootServices->HandleProtocol(lip->DeviceHandle,&efi_sfs_guid,(void**)&sfs);
	if (EFI_ERROR(s)){
		Print(L"Unable to load SFS!\r\n");
		goto _end;
	}
	EFI_FILE_PROTOCOL* r_fs;
	s=sfs->OpenVolume(sfs,&r_fs);
	if (EFI_ERROR(s)){
		Print(L"Unable to open Root Volume!\r\n");
		goto _end;
	}
	EFI_FILE_PROTOCOL* kf;
	s=r_fs->Open(r_fs,&kf,kernel_fp,EFI_FILE_MODE_READ,0);
	if (EFI_ERROR(s)){
		Print(L"Unable to open Kernel File!\r\n");
		goto _end;
	}
	uint8_t kf_i_bf[sizeof(EFI_FILE_INFO)+128];
	sz=sizeof(EFI_FILE_INFO)+128;
	s=kf->GetInfo(kf,&efi_fi_guid,&sz,&kf_i_bf);
	if (EFI_ERROR(s)){
		Print(L"Unable to Retrive Kernel File Info!\r\n");
		goto _end;
	}
	EFI_FILE_INFO* kf_i=(EFI_FILE_INFO*)kf_i_bf;
	if (kf_i->FileSize>MAX_KERNEL_FILE_SIZE){
		Print(L"Kernel Too Big! (0x%llx > 0x%llx)\r\n",kf_i->FileSize,MAX_KERNEL_FILE_SIZE);
		goto _end;
	}
	UEFI_LOADER_LOG(L"Kernel File Size: %llu (%llu Pages)\r\n",kf_i->FileSize,(kf_i->FileSize+PAGE_4KB_SIZE-1)>>PAGE_4KB_POWER_OF_2);
	void* k_dt;
	s=st->BootServices->AllocatePages(AllocateAnyPages,0x80000000,(kf_i->FileSize+PAGE_4KB_SIZE-1)>>PAGE_4KB_POWER_OF_2,(EFI_PHYSICAL_ADDRESS*)&k_dt);
	if (EFI_ERROR(s)){
		Print(L"Unable to Allocate Memory for Kernel!\r\n");
		goto _end;
	}
	s=kf->Read(kf,&kf_i->FileSize,k_dt);
	FreePool(kf_i);
	if (EFI_ERROR(s)){
		Print(L"Unable to Read Kernel into Memory!\r\n");
		goto _end;
	}
	struct ELF_HEADER* kh=(struct ELF_HEADER*)k_dt;
	if (kh->m!=ELF_HEADER_MAGIC){
		Print(L"Invalid ELF Magic: 0x%lx\r\n",kh->m);
		goto _end;
	}
	if (kh->ws!=ELF_HEADER_WORD_SIZE){
		Print(L"Unsupported ELF Word Size: %u\r\n",kh->ws);
		goto _end;
	}
	if (kh->ed!=ELF_HEADER_ENDIANESS){
		Print(L"Unsupported ELF Endianess: %u\r\n",kh->ed);
		goto _end;
	}
	if (kh->hv!=ELF_HEADER_VERSION){
		Print(L"Unsupported ELF Header Version: %u\r\n",kh->hv);
		goto _end;
	}
	if (kh->abi!=ELF_HEADER_OS_ABI){
		Print(L"Unsupported ELF OS ABI: %u\r\n",kh->abi);
		goto _end;
	}
	if (kh->tm!=ELF_HEADER_TAREGT_MACHINE){
		Print(L"Unsupported ELF Target Machine: %u\r\n",kh->tm);
		goto _end;
	}
	struct ELF_PROGRAM_HEADER* k_ph=(struct ELF_PROGRAM_HEADER*)((uint8_t*)k_dt+kh->ph_off);
	uint64_t pb=-1;
	uint64_t pe=0;
	for (uint16_t i=0;i<kh->ph_n;i++){
		if ((k_ph+i)->t!=ELF_PT_LOAD){
			continue;
		}
		if (pb!=-1){
			Print(L"Multiple PT_LOAD Sections not Supported!\r\n");
			goto _end;
		}
		if ((k_ph+i)->va<pb){
			pb=(k_ph+i)->va;
		}
		if ((k_ph+i)->va+(k_ph+i)->m_sz>pe){
			pe=(k_ph+i)->va+(k_ph+i)->m_sz;
		}
	}
	ka->t_pg=2+PAGE_TABLE_ENTRIES;
	ka->u_pg=1;
	uint16_t li[4]={-1,-1,-1,0};
	for (uint64_t i=pb;i<pe;i+=PAGE_4KB_SIZE){
		for (uint8_t k=0;k<3;k++){
			uint16_t l=(i>>(39-9*k))&0x1ff;
			if (li[k]!=l){
				if (k){
					ka->t_pg++;
				}
				li[k]=l;
			}
		}
	}
	ka->t_pg=((ka->t_pg+PAGE_TABLE_ENTRIES-1)>>(PAGE_2MB_POWER_OF_2-PAGE_4KB_POWER_OF_2))<<(PAGE_2MB_POWER_OF_2-PAGE_4KB_POWER_OF_2);
	uint64_t k_pg=(pe-pb+PAGE_4KB_SIZE-1)>>PAGE_4KB_POWER_OF_2;
	uint64_t* k_pg_pa=AllocatePool((k_pg+KERNEL_DATA_PAGE_COUNT)*sizeof(uint64_t));
	uint64_t i=0;
	j=0;
	uint64_t k=KERNEL_MEM_MAP_GET_BASE(ka->mmap[0].b);
	while (i<k_pg+KERNEL_DATA_PAGE_COUNT){
		if (k>=KERNEL_MEM_MAP_GET_BASE(ka->mmap[j].b)+ka->mmap[j].l){
			j++;
			if (j>=ka->mmap_l){
				Print(L"Not enought Memory to Map the Kernel\r\n");
				goto _end;
			}
			k=KERNEL_MEM_MAP_GET_BASE(ka->mmap[j].b);
		}
		*(k_pg_pa+i)=k;
		UEFI_LOADER_LOG(L"Page[%llu] = %llx\r\n",i,k);
		i++;
		k+=PAGE_4KB_SIZE;
	}
	if (k>=KERNEL_MEM_MAP_GET_BASE(ka->mmap[j].b)+ka->mmap[j].l){
		j++;
		if (j>=ka->mmap_l){
			Print(L"Not enought Memory to Create the next Physical Address\r\n");
			goto _end;
		}
		k=KERNEL_MEM_MAP_GET_BASE(ka->mmap[j].b);
	}
	ka->n_pa=k;
	ka->n_pa_idx=j;
	uint64_t ke=kh->e;
	UEFI_LOADER_LOG(L"Kernel: %llx -> +%llx; Entrypoint: %llx\r\n",pb,pe-pb,ke);
	j=0;
	k=-1;
	for (i=0;i<k_pg;i++){
		while ((k_ph+j)->t!=ELF_PT_LOAD){
			j++;
		}
		if (k==-1){
			k=(k_ph+j)->f_sz;
		}
		st->BootServices->CopyMem((void*)*(k_pg_pa+i),(uint8_t*)k_dt+(k_ph+j)->off+i*PAGE_4KB_SIZE,(k>PAGE_4KB_SIZE?PAGE_4KB_SIZE:k));
		UEFI_LOADER_LOG(L"Kernel Section#%llu: %llx => %llx -> %llx\r\n",j,(uint8_t*)k_dt+(k_ph+j)->off+i*PAGE_4KB_SIZE,*(k_pg_pa+i),(k_ph+j)->va+i*PAGE_4KB_SIZE);
		k-=PAGE_4KB_SIZE;
	}
	s=st->BootServices->FreePages((EFI_PHYSICAL_ADDRESS)k_dt,(kf_i->FileSize+PAGE_4KB_SIZE-1)>>PAGE_4KB_POWER_OF_2);
	if (EFI_ERROR(s)){
		Print(L"Error Freeing Kernel File Pages!\r\n");
		goto _end;
	}
	ka->pml4=(uint64_t*)PML4_PHYSICAL_ADDRESS;
	s=st->BootServices->AllocatePages(AllocateAddress,0x80000000,ka->t_pg<<PAGE_4KB_POWER_OF_2,(EFI_PHYSICAL_ADDRESS*)&ka->pml4);
	uint64_t* cr3=asm_clear_pages_get_cr3((uint64_t)ka->pml4,ka->t_pg);
	UEFI_LOADER_LOG(L"PML4 PA Pointer: %llx\r\nSetting Up Tables...\r\n",ka->pml4);
	for (i=0;i<PAGE_TABLE_ENTRIES/2;i++){
		*(ka->pml4+i)=*(cr3+i);
	}
	li[0]=-1;
	li[1]=-1;
	li[2]=-1;
	li[3]=-1;
	uint64_t* pt[4]={ka->pml4,NULL,NULL,NULL};
	j=0;
	ka->k_sp=0;
	for (i=pb;i<pe+IDT_SIZE+KERNEL_STACK_SIZE;i+=PAGE_4KB_SIZE){
		for (k=0;k<4;k++){
			uint16_t l=(i>>(39-9*k))&0x1ff;
			if (li[k]!=l){
				li[k]=l;
				if (k<3){
					UEFI_LOADER_LOG(L"New Page Table: #%lu [%u] -> %llx\r\n",ka->u_pg,k+1,ka->pml4+ka->u_pg*PAGE_TABLE_ENTRIES);
					pt[k+1]=(uint64_t*)((uint8_t*)ka->pml4+(ka->u_pg<<PAGE_4KB_POWER_OF_2));
					*(pt[k]+l)=((uint64_t)pt[k+1])|PAGE_DIR_READ_WRITE|PAGE_DIR_PRESENT;
					ka->u_pg++;
				}
				else{
					*(pt[3]+l)=(*(k_pg_pa+j))|PAGE_READ_WRITE|PAGE_PRESENT;
					j++;
				}
			}
		}
		if (i<pe){
			UEFI_LOADER_LOG(L"Kernel 4KB Page: [%u : %u : %u : %u] -> %llx (%llx)\r\n",li[0],li[1],li[2],li[3],i,*(k_pg_pa+j-1));
		}
		else if (i<pe+IDT_SIZE){
			UEFI_LOADER_LOG(L"IDT 4KB Page: [%u : %u : %u : %u] -> %llx (%llx)\r\n",li[0],li[1],li[2],li[3],i,*(k_pg_pa+j-1));
			ka->idt=(void*)i;
		}
		else{
			UEFI_LOADER_LOG(L"Kernel Stack 4KB Page: [%u : %u : %u : %u] -> %llx (%llx)\r\n",li[0],li[1],li[2],li[3],i,*(k_pg_pa+j-1));
			if (!ka->k_sp){
				ka->k_sp=i+KERNEL_STACK_SIZE;
			}
		}
	}
	uint64_t pml4_va=pb+((k_pg+KERNEL_DATA_PAGE_COUNT)<<PAGE_4KB_POWER_OF_2);
	pml4_va+=PAGE_2MB_SIZE-(pml4_va&(PAGE_2MB_SIZE-1));
	UEFI_LOADER_LOG(L"Page Table Address: %llx - +%llx (%llu Tables) -> [%u : %u : %u : %u]\r\n",pml4_va,ka->t_pg<<PAGE_4KB_POWER_OF_2,ka->t_pg,(pml4_va>>39)&0x1ff,(pml4_va>>PAGE_1GB_POWER_OF_2)&0x1ff,(pml4_va>>PAGE_2MB_POWER_OF_2)&0x1ff,(pml4_va>>PAGE_4KB_POWER_OF_2)&0x1ff);
	ka->n_va=pml4_va+(ka->t_pg<<PAGE_4KB_POWER_OF_2);
	ka->va_to_pa=pml4_va-(uint64_t)(void*)ka->pml4;
	if ((ka->t_pg>>(PAGE_2MB_POWER_OF_2-PAGE_4KB_POWER_OF_2))>PAGE_TABLE_ENTRIES){
		Print(L"Too Many Page Tables! (%llu / %llu)\r\n",ka->t_pg,PAGE_TABLE_ENTRIES<<(PAGE_2MB_POWER_OF_2-PAGE_4KB_POWER_OF_2));
		goto _end;
	}
	for (k=0;k<3;k++){
		uint16_t l=(pml4_va>>(39-9*k))&0x1ff;
		if (li[k]!=l){
			li[k]=l;
			if (k<2){
				UEFI_LOADER_LOG(L"New Page Table: #%lu [%u] -> %llx\r\n",ka->u_pg,k+1,ka->pml4+ka->u_pg*PAGE_TABLE_ENTRIES);
				pt[k+1]=(uint64_t*)((uint8_t*)ka->pml4+(ka->u_pg<<PAGE_4KB_POWER_OF_2));
				*(pt[k]+l)=((uint64_t)pt[k+1])|PAGE_DIR_READ_WRITE|PAGE_DIR_PRESENT;
				ka->u_pg++;
			}
		}
	}
	for (i=0;i<(ka->t_pg>>(PAGE_2MB_POWER_OF_2-PAGE_4KB_POWER_OF_2));i++){
		UEFI_LOADER_LOG(L"Page Table 2MB Page: [%u : %u : %u] -> %llx\r\n",li[0],li[1],li[2]+i,pml4_va+(i<<PAGE_2MB_POWER_OF_2));
		*(pt[2]+li[2]+i)=(PML4_PHYSICAL_ADDRESS+(i<<PAGE_2MB_POWER_OF_2))|PAGE_DIR_SIZE|PAGE_DIR_READ_WRITE|PAGE_DIR_PRESENT;
	}
	UEFI_LOADER_LOG(L"Extra Page Tables: %llu (%llu Used)\r\n",ka->t_pg-ka->u_pg,ka->u_pg);
	mm_sz=sizeof(EFI_MEMORY_DESCRIPTOR)*32;
	mm_k=0;
	mm_ds=0;
	mm_v=0;
	bf=NULL;
	do{
		s=st->BootServices->AllocatePool(0x80000000,mm_sz,(void**)&bf);
		if (EFI_ERROR(s)){
			Print(L"This shouldn't happen?\r\n");
			goto _end;
		}
		if (bf==NULL){
			break;
		}
		s=st->BootServices->GetMemoryMap(&mm_sz,(EFI_MEMORY_DESCRIPTOR*)bf,&mm_k,&mm_ds,&mm_v);
		if (s!=EFI_SUCCESS){
			st->BootServices->FreePool(bf);
			mm_sz+=sizeof(EFI_MEMORY_DESCRIPTOR)*32;
		}
	} while (s!=EFI_SUCCESS);
	s=st->BootServices->ExitBootServices(ih,mm_k);
	if (EFI_ERROR(s)){
		Print(L"Failed to Exit EFI Loader!\r\n");
		goto _end;
	}
	for (i=0;i<mm_sz/mm_ds;i++){
		((EFI_MEMORY_DESCRIPTOR*)bf)->VirtualStart=((EFI_MEMORY_DESCRIPTOR*)bf)->PhysicalStart;
	}
	asm_enable_paging((uint64_t)ka->pml4);
	ka->pml4=(uint64_t*)(void*)pml4_va;
	s=st->RuntimeServices->SetVirtualAddressMap(mm_sz,mm_ds,mm_v,(EFI_MEMORY_DESCRIPTOR*)bf);
	if (EFI_ERROR(s)){
		Print(L"Failed to Virtualize EFI!\r\n");
		goto _end;
	}
	((kmain)ke)(ka);
_end:
	asm_halt();
}
