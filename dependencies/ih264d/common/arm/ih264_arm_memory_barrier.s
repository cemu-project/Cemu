@/******************************************************************************
@ *
@ * Copyright (C) 2015 The Android Open Source Project
@ *
@ * Licensed under the Apache License, Version 2.0 (the "License");
@ * you may not use this file except in compliance with the License.
@ * You may obtain a copy of the License at:
@ *
@ * http://www.apache.org/licenses/LICENSE-2.0
@ *
@ * Unless required by applicable law or agreed to in writing, software
@ * distributed under the License is distributed on an "AS IS" BASIS,
@ * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@ * See the License for the specific language governing permissions and
@ * limitations under the License.
@ *
@ *****************************************************************************
@ * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
@*/
@**
@*******************************************************************************
@* @file
@*  ih264_arm_memory_barrier.s
@*
@* @brief
@*  Contains function definitions for data synchronization.
@*
@* @author
@*  Ittiam
@*
@* @par List of Functions:
@*
@*
@* @remarks
@*  None
@*
@*******************************************************************************

.text
.p2align 2

@*****************************************************************************
@*
@* Function Name    : ih264_arm_dsb
@* Description      : Adds DSB
@* Revision History  :
@*        DD MM YYYY    Author(s)   Changes
@*        03 07 2008    100355      First version
@*
@*****************************************************************************

        .global ih264_arm_dsb
ih264_arm_dsb:
    dsb
    bx            lr



@*****************************************************************************
@*
@* Function Name    : ih264_arm_dmb
@* Description      : Adds DMB
@* Revision History  :
@*        DD MM YYYY    Author(s)   Changes
@*        03 07 2008    100355      First version
@*
@*****************************************************************************

        .global ih264_arm_dmb

ih264_arm_dmb:
    dmb
    bx            lr



