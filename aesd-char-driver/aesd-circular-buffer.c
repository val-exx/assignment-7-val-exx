<<<<<<< HEAD
/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    
    size_t global_offset=0;
    uint8_t entry_index;
    uint8_t count;
    
    //check if pointers are valid
    if(buffer == NULL || entry_offset_byte_rtn == NULL) return NULL;
    
    
    //compute the index. if circular buffer is full, the index is the max, otherwise
    //is the entry where the next  write should be stored
    count = buffer->full ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED : buffer->in_offs;
    
    for (uint8_t i=0;i<count;i++){
    	entry_index = (buffer->out_offs+i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    	struct aesd_buffer_entry *entry = &buffer->entry[entry_index];
    	if(char_offset<global_offset+entry->size){
    		*entry_offset_byte_rtn=char_offset-global_offset;
    		return entry;
    	}
    	global_offset+=entry->size;
    }
    
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    
    //check if pointers are valid
    if(buffer == NULL || add_entry == NULL) return;
    
    
    //if the buffer is full, the entry at in_offs is the oldest one, so it will be overwritten.
    //out_offs will be updated because the oldest entry has been replaced
    if(buffer->full) buffer->out_offs=(buffer->out_offs+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    
    buffer->entry[buffer->in_offs]=*add_entry;
    
    //update in_offs index
    buffer->in_offs=(buffer->in_offs+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    //when in_offs matches out_offs, the buffer becomes full, so update the flag
    if(buffer->in_offs==buffer->out_offs) buffer->full=true;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
=======

>>>>>>> d98d9f82f055ce00710293578d8ac2090afcf49b
