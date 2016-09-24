/******************************************************************************
 *    Copyright (C) 2014 Hisilicon STB Development Dept
 *    All rights reserved.
 * ***
 *    Create by Cai Zhiyong
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
******************************************************************************/

#ifndef HIUAPIH
#define HIUAPIH

#define HIUAPI_GET_RAM_SIZE       1
#define HIUAPI_GET_CMA_SIZE       2
/**
 * get memory size information, such CMA size, RAM size.
 *
 * param:
 *   size    specify size,
 *   flags   what size you want get.
 *           HIUAPI_GET_RAM_SIZE - get ram size, unit is 1M
 *           HIUAPI_GET_CMA_SIZE - get cma size, unit is 1M
 * retval:
 *   0       success
 *   other   fail.
 */
int get_mem_size(unsigned int *size, int flags);

#endif /* HIUAPIH */
