/*
 * MSSIGN32 implementation
 *
 * Copyright 2009 Austin English
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define WIN32_NO_STATUS

#include <config.h>

#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <wincrypt.h>

#include <wine/debug.h>

WINE_DEFAULT_DEBUG_CHANNEL(mssign);

BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID lpv )
{
    switch(reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hinst );
        break;
    }
    return TRUE;
}


HRESULT WINAPI PvkGetCryptProv(HWND hwnd, LPCWSTR pwszCaption, LPCWSTR pwszCapiProvider,
                    DWORD dwProviderType, LPCWSTR pwszPvkFile, LPCWSTR pwszKeyContainerName,
                    DWORD *pdwKeySpec, LPWSTR *ppwszTmpContainer, HCRYPTPROV *phCryptProv)
{
    FIXME("%p %s %s %d %s %s %p %p %p stub\n", hwnd, debugstr_w(pwszCaption), debugstr_w(pwszCapiProvider),
                    dwProviderType, debugstr_w(pwszPvkFile), debugstr_w(pwszKeyContainerName),
                    pdwKeySpec, ppwszTmpContainer, phCryptProv);

    return E_FAIL;
}

BOOL WINAPI PvkPrivateKeyAcquireContextFromMemory(LPCWSTR pwszProvName, DWORD dwProvType,
                    BYTE *pbData, DWORD cbData, HWND hwndOwner, LPCWSTR pwszKeyName,
                    DWORD *pdwKeySpec, HCRYPTPROV *phCryptProv, LPWSTR *ppwszTmpContainer)
{
    FIXME("%s %d %p %d %p %s %p %p %p stub\n", debugstr_w(pwszProvName), dwProvType,
                    pbData, cbData, hwndOwner, debugstr_w(pwszKeyName), pdwKeySpec,
                    phCryptProv, ppwszTmpContainer);

    return FALSE;
}

void WINAPI PvkFreeCryptProv(HCRYPTPROV hProv, LPCWSTR pwszCapiProvider, DWORD dwProviderType,
                    LPWSTR pwszTmpContainer)
{
    FIXME("%08lx %s %d %s stub\n", hProv, debugstr_w(pwszCapiProvider), dwProviderType,
                    debugstr_w(pwszTmpContainer));
}
