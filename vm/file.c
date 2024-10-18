/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	if (page == NULL)
		return;

	struct file_page *file_page UNUSED = &page->file;
	// file_close(file_page->file);
}

static bool lazy_load_segment_mmap (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */

	/* NOTE: The beginning where custom code is added */
	struct file_page *info = (struct file_page *)aux;
    struct file *file = info->file;
    off_t offset = info->offset;
    size_t page_read_bytes = info->page_read_bytes;
    size_t page_zero_bytes = info->page_zero_bytes;

	page->file.file = file;
	page->file.page_read_bytes = page_read_bytes;
	/* Allocate a physical frame */
    uint8_t *kva = page->frame->kva;

	/* Read from file */
    file_seek(file, offset);
    if (file_read(file, kva, page_read_bytes) != (int)page_read_bytes) {
        /* Handle read error */
        palloc_free_page(kva);
        return false;
    }

	/* Zero the remaining bytes */
    memset(kva + page_read_bytes, 0, page_zero_bytes);
    return true;
	/* NOTE: The end where custom code is added */
}

void *
mmap_file (void *addr, size_t length, int writable, int fd, off_t offset) {
    if (offset % PGSIZE != 0 || pg_round_down(addr) != addr || is_kernel_vaddr(addr) ||
		addr == NULL || (long)length <= 0 || (addr + length >= KERN_BASE))
        return NULL;
	
    if (fd == 0 || fd == 1)
        exit(-1);
    
    if (spt_find_page(&thread_current()->spt, addr))
        return NULL;

    struct file *file = thread_current()->fd_table[fd];
    if (file == NULL)
        return NULL;
		
    return do_mmap(addr, length, writable, file, offset);
}

/* Do the mmap */
static void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	/* NOTE: The beginning where custom code is added */
	struct file *mmap_file = file_reopen(file);
  	void * first_addr = addr;
  	size_t read_bytes = length > file_length(file) ? file_length(file) : length;
  	size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_page *info = (struct file_page *)malloc(sizeof(struct file_page));
		info->file = mmap_file;
		info->offset = offset;
		info->page_read_bytes = page_read_bytes;
		info->page_zero_bytes = page_zero_bytes;
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_segment_mmap, info)) {
			return NULL;
    	}
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr       += PGSIZE;
		offset     += page_read_bytes;
	}

	return first_addr;
	/* NOTE: The end where custom code is added */
}

void munmap(void *addr) {
	do_munmap(addr);
}

/* Do the munmap */
static void 
do_munmap(void *addr) {
	struct page* page = NULL;
	off_t offset = 0;
    while (true) {
		page = spt_find_page(&thread_current()->spt, addr);
		if (page == NULL) 
			break;
		if (page->operations->type != VM_FILE)
			break;

		/* 수정된 페이지(dirty bit == 1)는 파일에 업데이트해놓는다. 이후에 dirty bit을 0으로 만든다. */
		if (pml4_is_dirty(thread_current()->pml4, page->va)){
			file_write_at(page->file.file, addr, page->file.page_read_bytes, offset);
			pml4_set_dirty(thread_current()->pml4, page->va, 0);
		}
		
		pml4_clear_page(thread_current()->pml4, page->va);
		addr += PGSIZE;
		offset += PGSIZE;
    }
	// file_backed_destroy(page);

	// struct thread *cur = thread_current();
	// struct page *mun_page = NULL;
	// void *first_va = addr;
	// off_t offset = 0;
	// while ((mun_page = spt_find_page(&cur->spt, addr) != NULL)) {
	// 	if (mun_page->operations->type != VM_FILE)
	// 		break;
	// 	if (mun_page->file.mmap_ptr != first_va)
	// 		break;
	// 	if (pml4_is_dirty(cur->pml4, addr)) {
	// 		file_write(&mun_page->file.file, mun_page->frame->kva, PGSIZE);
	// 		spt_remove_page(&cur->spt, mun_page);
	// 	}
	// 	addr += PGSIZE;
	// 	offset += PGSIZE;
	// }
	// file_backed_destroy(mun_page);
}
