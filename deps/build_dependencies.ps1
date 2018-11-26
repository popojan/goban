(new-object System.Net.WebClient).DownloadFile(
	'https://download.savannah.gnu.org/releases/freetype/freetype-2.9.tar.gz',
	'freetype-2.9.tar.gz'
)

(Get-ItemProperty -Path HKLM:\SOFTWARE\Microsoft\MSBuild\ToolsVersions\12.0 -Name MSBuildToolsPath).M
SBuildToolsPath