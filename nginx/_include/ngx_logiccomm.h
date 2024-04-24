#ifndef __NGX_LOGICCOMM_H__
#define __NGX_LOGICCOMM_H__

#pragma pack(1)

typedef struct _STRUCT_REGISTER
{
    int     iType;
    char    username[56];
    char    password[40];
}STRUCT_REGISTER,*LPSTRUCT_REGISTER;

typedef struct _STRUCT_LOGIN
{
    char    username[56];
    char    password[40];
}STRUCT_LOGIN,*LPSTRUCT_LOGIN;

#pragma pack()

#endif

