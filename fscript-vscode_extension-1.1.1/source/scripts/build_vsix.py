#!/usr/bin/env python3
"""Package this dependency-free extension offline as a universal VSIX.

The package uses the VSIX 2.0 container and Microsoft.VisualStudio.Code.Manifest
asset expected by the VS Code extension installer. No Marketplace upload.
"""
from pathlib import Path
import json
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / 'package.json').read_text())
namespace = 'http://schemas.microsoft.com/developer/vsx-schema/2011'
ET.register_namespace('', namespace)
def tag(name): return '{' + namespace + '}' + name
def node(parent, name, attributes=None, text=None):
    element = ET.SubElement(parent, tag(name), attributes or {})
    element.text = text
    return element

package = ET.Element(tag('PackageManifest'), {'Version': '2.0.0'})
metadata = node(package, 'Metadata')
node(metadata, 'Identity', {'Language': 'en-US', 'Id': manifest['name'], 'Version': manifest['version'], 'Publisher': manifest['publisher']})
node(metadata, 'DisplayName', text=manifest['displayName'])
node(metadata, 'Description', {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'}, manifest['description'])
node(metadata, 'Tags', text=','.join(manifest.get('keywords', [])))
node(metadata, 'Categories', text=','.join(manifest.get('categories', [])))
node(metadata, 'GalleryFlags', text='Public')
properties = node(metadata, 'Properties')
for key, value in {
    'Microsoft.VisualStudio.Code.Engine': manifest['engines']['vscode'],
    'Microsoft.VisualStudio.Code.ExtensionKind': ','.join(manifest.get('extensionKind', ['workspace'])),
    'Microsoft.VisualStudio.Code.ExecutesCode': 'true',
    'Microsoft.VisualStudio.Code.EnabledApiProposals': '',
    'Microsoft.VisualStudio.Code.ExtensionDependencies': '',
    'Microsoft.VisualStudio.Code.ExtensionPack': '',
    'Microsoft.VisualStudio.Code.LocalizedLanguages': '',
    'Microsoft.VisualStudio.Code.PreRelease': 'false',
}.items():
    node(properties, 'Property', {'Id': key, 'Value': value})
installation = node(package, 'Installation')
node(installation, 'InstallationTarget', {'Id': 'Microsoft.VisualStudio.Code'})
node(package, 'Dependencies')
assets = node(package, 'Assets')
for asset, file in [('Microsoft.VisualStudio.Code.Manifest', 'package.json'), ('Microsoft.VisualStudio.Services.Content.Details', 'README.md'), ('Microsoft.VisualStudio.Services.Content.License', 'LICENSE'), ('Microsoft.VisualStudio.Services.Content.Changelog', 'CHANGELOG.md')]:
    node(assets, 'Asset', {'Type': asset, 'Path': 'extension/' + file, 'Addressable': 'true'})

content_namespace = 'http://schemas.openxmlformats.org/package/2006/content-types'
content = ET.Element('Types', {'xmlns': content_namespace})
for extension, content_type in [('json', 'application/json'), ('js', 'application/javascript'), ('md', 'text/markdown'), ('ui', 'text/plain'), ('fscript', 'text/plain'), ('vsixmanifest', 'text/xml'), ('txt', 'text/plain'), ('cpp', 'text/plain'), ('h', 'text/plain')]:
    ET.SubElement(content, 'Default', {'Extension': extension, 'ContentType': content_type})
ET.SubElement(content, 'Override', {'PartName': '/extension/LICENSE', 'ContentType': 'text/plain'})
for file in (ROOT/'bin').rglob('*') if (ROOT/'bin').exists() else []:
    if file.is_file(): ET.SubElement(content, 'Override', {'PartName':'/extension/'+str(file.relative_to(ROOT)), 'ContentType':'application/octet-stream'})

files = [ROOT / name for name in ['package.json', 'README.md', 'LICENSE', 'CHANGELOG.md', 'fscript-language-configuration.json', 'ui-language-configuration.json']]
for directory in ['src', 'syntaxes', 'snippets', 'examples', 'toolchain', 'bin']:
    files.extend(p for p in (ROOT / directory).rglob('*') if p.is_file())
for contribution in manifest['contributes']['grammars'] + manifest['contributes']['snippets']:
    assert (ROOT / contribution['path']).is_file(), contribution['path']
assert (ROOT / manifest['main']).is_file()
for file in files:
    if file.suffix == '.json': json.loads(file.read_text())
output = ROOT / 'dist' / f"{manifest['name']}-{manifest['version']}.vsix"
output.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('extension.vsixmanifest', ET.tostring(package, encoding='utf-8', xml_declaration=True))
    archive.writestr('[Content_Types].xml', ET.tostring(content, encoding='utf-8', xml_declaration=True))
    for file in sorted(files): archive.write(file, 'extension/' + str(file.relative_to(ROOT)))
with zipfile.ZipFile(output) as archive:
    assert archive.testzip() is None
    packaged = json.loads(archive.read('extension/package.json'))
    assert packaged['version'] == manifest['version']
    assert 'extension/' + packaged['main'].removeprefix('./') in archive.namelist()
    ET.fromstring(archive.read('extension.vsixmanifest'))
    ET.fromstring(archive.read('[Content_Types].xml'))
print(output)
