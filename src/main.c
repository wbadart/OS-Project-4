/*
   Main program for the virtual memory project.
   Make all of your modifications to this file.
   You may add or rearrange any code or data as you need.
   The header files page_table.h and disk.h explain
   how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


char *algorithm;
int *frame_states
    , frame_queue_index
    , npages        //pages represent program in virtual memory
    , nframes;      //frames = physical memory

struct disk *disk;

void page_fault_handler( struct page_table *pt, int page )
{
    printf("page fault on page #%d\n",page);

    // Determine if there's an empty frame
    int target_frame;
    for(target_frame = frame_queue_index + 1;
            target_frame != frame_queue_index;
            target_frame = (target_frame + 1) % nframes)
        if(!frame_states[target_frame]) break;

    // If i != frame_queue_index, i points to an empty frame since
    // it wrapped around the whole queue without finding empty frame
    // If i doesn't point to an empty frame, evict
    if(target_frame == frame_queue_index){

        int eviction_target;

        // `rand` just picks a random frame
        if(strncmp(algorithm, "rand", 7) == 0)
            eviction_target = rand() % nframes;

        // `fifo` evicts the earliest-inserted frame, which
        // is tracked by frame_queue_index
        else if(strncmp(algorithm, "fifo", 7) == 0)
            eviction_target = frame_queue_index;

        // `custom` does some cool stuff...
        else if(strncmp(algorithm, "custom", 7) == 0)
            eviction_target = rand() % nframes;

        else {
            fprintf( stderr
                    , "ERR: algorithm '%s' not recognized\n"
                    , algorithm );
            exit(2);
        }

        // Set target frame to eviction_target
        target_frame = eviction_target;

        //once target page is selected, check if it has been written to
        int bits = 0;
        int frame = 0;
        page_table_get_entry(pt, target_frame, &frame, &bits);
        //just PROT_WRITE        = 010 = 2
        //just PROT_READ         = 100 = 4
        //PROT_READ & PROT_WRITE = 110 = 6
        if(bits == 2 || bits == 6){
            //write target page to disk before kicking it out
            disk_write(disk, target_frame, page_table_get_physmem(pt) + frame);
        }
        //set frame to new page that got read in
        page_table_set_entry(pt, target_frame, frame, PROT_READ);
        frame_states[frame] = 1;

    }

    // read something from disk into frame
    disk_read(disk, page, page_table_get_physmem(pt) + target_frame);
    page_table_set_entry(pt, page, target_frame, PROT_READ);
    frame_states[target_frame] = 1;
    frame_queue_index = target_frame;

}

int main( int argc, char *argv[] )
{

    if(argc!=5) {
        printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
        return 1;
    }

    npages = atoi(argv[1]);
    nframes = atoi(argv[2]);
    algorithm = argv[3];
    const char *program = argv[4];

    frame_states = malloc(nframes * sizeof(int));
    frame_queue_index = 0;

    int i;
    for(i = 0; i < nframes; i++) frame_states[i] = 0;

    disk = disk_open("myvirtualdisk",npages);
    if(!disk) {
        fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
        return 1;
    }

    struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
    if(!pt) {
        fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
        return 1;
    }

    char *virtmem = page_table_get_virtmem(pt);
    char *physmem = page_table_get_physmem(pt);

    if(!strcmp(program,"sort")) {
        sort_program(virtmem,npages*PAGE_SIZE);

    } else if(!strcmp(program,"scan")) {
        scan_program(virtmem,npages*PAGE_SIZE);

    } else if(!strcmp(program,"focus")) {
        focus_program(virtmem,npages*PAGE_SIZE);

    } else {
        fprintf(stderr,"ERR: unknown program: %s\n",argv[3]);
        return 1;
    }

    free(frame_states);
    page_table_delete(pt);
    disk_close(disk);

    return 0;
}
