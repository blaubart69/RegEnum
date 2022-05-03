Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;
 
public static class Bumsti
{
    [DllImport("user32.dll", CharSet=CharSet.Auto)]
        public static extern bool MessageBox(
            IntPtr hWnd,     /// Parent window handle 
            String text,     /// Text message to display
            String caption,  /// Window caption
            int options);    /// MessageBox type

    [DllImport("advapi32.dll", CharSet = CharSet.Auto)]
            extern public static int RegQueryInfoKey(
                UIntPtr hkey,
                StringBuilder lpClass,
                ref uint lpcbClass,
                IntPtr lpReserved,
                out uint lpcSubKeys,
                out uint lpcbMaxSubKeyLen,
                out uint lpcbMaxClassLen,
                out uint lpcValues,
                out uint lpcbMaxValueNameLen,
                out uint lpcbMaxValueLen,
                out uint lpcbSecurityDescriptor,
                out long lpftLastWriteTime
            );
}
"@
 
[Bumsti]::MessageBox(0,"Text","Caption",0) |Out-Null