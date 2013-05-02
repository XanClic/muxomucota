/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <assert.h>
#include <ipc.h>
#include <vfs.h>


extern uintmax_t _vfs_create_pipe(void);
extern uintmax_t _vfs_destroy_pipe(void);
extern uintmax_t _vfs_duplicate_pipe(void);

extern uintmax_t _vfs_stream_recv(uintptr_t shmid);
extern uintmax_t _vfs_stream_send(uintptr_t shmid);

extern uintmax_t _vfs_stream_recv_msg(void);
extern uintmax_t _vfs_stream_send_msg(void);

extern uintmax_t _vfs_pipe_get_flag(void);
extern uintmax_t _vfs_pipe_set_flag(void);

extern uintmax_t _vfs_pipe_implements(void);

extern uintmax_t _vfs_is_symlink(uintptr_t shmid);


void _provide_vfs_services(void);

void _provide_vfs_services(void)
{
    popup_message_handler(CREATE_PIPE,     _vfs_create_pipe);
    popup_message_handler(DESTROY_PIPE,    _vfs_destroy_pipe);
    popup_message_handler(DUPLICATE_PIPE,  _vfs_duplicate_pipe);

    popup_shm_handler    (STREAM_RECV,     _vfs_stream_recv);
    popup_shm_handler    (STREAM_SEND,     _vfs_stream_send);

    popup_message_handler(STREAM_RECV_MSG, _vfs_stream_recv_msg);
    popup_message_handler(STREAM_SEND_MSG, _vfs_stream_send_msg);

    popup_message_handler(PIPE_GET_FLAG,   _vfs_pipe_get_flag);
    popup_message_handler(PIPE_SET_FLAG,   _vfs_pipe_set_flag);

    popup_message_handler(PIPE_IMPLEMENTS, _vfs_pipe_implements);

    popup_shm_handler    (IS_SYMLINK,      _vfs_is_symlink);
}
