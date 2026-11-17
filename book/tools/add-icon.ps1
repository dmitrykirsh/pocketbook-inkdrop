<# Adds/removes the InkDrop PB629 desktop tile, always keeping a rollback copy. #>
param([string]$Root,[switch]$Remove)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
public static class InkDropCrc {
  static uint[] T = Make();
  static uint[] Make() { var t=new uint[256]; for(uint i=0;i<256;i++){uint c=i;for(int j=0;j<8;j++)c=(c&1)!=0?0xEDB88320u^(c>>1):c>>1;t[i]=c;}return t; }
  public static uint Crc32(byte[] data) { uint c=0xffffffff; foreach(byte b in data)c=T[(c^b)&255]^(c>>8);return c^0xffffffff; }
}
'@
if(!$Root){$Root=(Get-PSDrive -PSProvider FileSystem|Where-Object{Test-Path (Join-Path $_.Root 'system\config\desktop\view.json')}|Select-Object -First 1).Root}
if(!$Root){throw 'PocketBook drive not found.'}
$d=Join-Path $Root 'system\config\desktop';$view=Join-Path $d 'view.json';$hash=Join-Path $d 'hash.txt';$utf8=New-Object Text.UTF8Encoding($false)
if($Remove){foreach($x in 'view.json','hash.txt'){if(Test-Path "$d\$x.inkdrop-backup"){Copy-Item "$d\$x.inkdrop-backup" "$d\$x" -Force}};Write-Output 'InkDrop desktop entry removed.';return}
foreach($p in 'applications\InkDrop.app','system\inkdrop\icons\desktop_app_inkdrop.bmp','system\inkdrop\icons\desktop_app_inkdrop_f.bmp'){if(!(Test-Path (Join-Path $Root $p))){throw "Missing $p"}}
foreach($x in 'view.json','hash.txt'){if(!(Test-Path "$d\$x.inkdrop-backup")){Copy-Item "$d\$x" "$d\$x.inkdrop-backup"}}
try {
  $v=[IO.File]::ReadAllText($view,$utf8);if($v.Contains('"U_inkdrop"')){Write-Output 'InkDrop already present.';return}
  $nl=if($v.Contains("`r`n")){"`r`n"}else{"`n"}
  $entry='        "U_inkdrop": {'+$nl+'            "focused_icon": {'+$nl+'                "path": "system/inkdrop/icons/desktop_app_inkdrop_f.bmp"'+$nl+'            },'+$nl+'            "icon": {'+$nl+'                "path": "system/inkdrop/icons/desktop_app_inkdrop.bmp"'+$nl+'            },'+$nl+'            "path": "/mnt/ext1/applications/InkDrop.app",'+$nl+'            "title": "InkDrop"'+$nl+'        },'+$nl
  $mark='    "applications": {'+$nl;$i=$v.IndexOf($mark);if($i-lt 0){throw 'applications block not found'};$v=$v.Insert($i+$mark.Length,$entry)
  $g=$v.IndexOf('"@General"');$a=$v.IndexOf('"apps": [',$g);$e=$v.IndexOf(']',$a);if($g-lt 0 -or $a-lt 0 -or $e-lt 0){throw 'General group not found'}
  $v=$v.Substring(0,$e).TrimEnd()+','+$nl+'                    "U_inkdrop"'+$nl+'                '+$v.Substring($e)
  $v|ConvertFrom-Json|Out-Null;[IO.File]::WriteAllText($view,$v,$utf8)
  $old=[IO.File]::ReadAllText($hash,$utf8);$h1=([regex]::Match($old,'hash1=([0-9a-f]+)')).Groups[1].Value;if(!$h1){throw 'hash format changed'}
  $h2=([BitConverter]::ToString([Security.Cryptography.MD5]::Create().ComputeHash([IO.File]::ReadAllBytes($view)))).Replace('-','').ToLower()
  $body="hash1=$h1`nhash2=$h2`n";$crc=[InkDropCrc]::Crc32($utf8.GetBytes($body));[IO.File]::WriteAllText($hash,$body+"`n#{0:x8}`n" -f $crc,$utf8)
  Write-Output 'Done. Safely eject the reader and reboot it.'
} catch { foreach($x in 'view.json','hash.txt'){if(Test-Path "$d\$x.inkdrop-backup"){Copy-Item "$d\$x.inkdrop-backup" "$d\$x" -Force}};throw }
