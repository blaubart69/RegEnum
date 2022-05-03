 $MethodDefinition = @’

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

‘@

 
$advapi32 = Add-Type -MemberDefinition $MethodDefinition -Name ‘advapi32’ -Namespace ‘advapi32’ -PassThru

[Microsoft.Win32.RegistryKey]$key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey("Software\7-zip")
Write-Host $key.Handle.DangerousGetHandle()

[long]$lpftLastWriteTime = 0;

$sb = [System.Text.StringBuilder]::new()
$result = $advapi32::RegQueryInfoKey( $key.Handle.DangerousGetHandle(), 
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [System.IntPtr]::Zero,
    [ref]$lpftLastWriteTime);
