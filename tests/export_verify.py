"""Validate C++-generated Office/PDF fixtures with independent readers; optional developer check."""
from pathlib import Path
from zipfile import ZipFile
import sys,posixpath
from lxml import etree
from docx import Document
from pptx import Presentation
from openpyxl import load_workbook
import fitz
from fontTools.ttLib import TTFont
root=Path(sys.argv[1])
for ext in ['docx','pptx','xlsx','zip']:
 z=ZipFile(root/f'sample.{ext}');assert z.testzip() is None
 if ext=='zip':continue
 names=set(z.namelist())
 for name in names:
  if name.endswith('.xml') or name.endswith('.rels'):
   part=etree.fromstring(z.read(name))
   if name.endswith('.rels'):
    base='' if name=='_rels/.rels' else posixpath.dirname(posixpath.dirname(name))
    for rel in part:
     if rel.get('TargetMode')=='External':continue
     target=posixpath.normpath(posixpath.join(base,rel.get('Target')))
     assert target in names,(name,target)
doc=Document(root/'sample.docx');assert len(doc.tables)==1 and len(doc.tables[0].columns)==2
assert doc.tables[0].cell(1,1).text=='星际舰队'
assert any(run.bold for para in doc.paragraphs for run in para.runs)
prs=Presentation(root/'sample.pptx');assert len(prs.slides)==2
assert any('科学考察' in shape.text for shape in prs.slides[0].shapes if shape.has_text_frame)
wb=load_workbook(root/'sample.xlsx');assert len(wb.worksheets)==2
assert wb.worksheets[0]['B4'].value=='=SUM(B2:B3)'
assert wb.worksheets[0]['B5'].data_type=='s'
assert wb.worksheets[1]['B3'].value=='你好'
font=TTFont(root/'rebuilt.ttf',checkChecksums=2);assert ord('星') in font.getBestCmap();assert fitz.open(root/'empty.pdf')[0].get_text()==''
pdf=fitz.open(root/'sample.pdf');text=''.join(page.get_text() for page in pdf)
assert 'Starfleet' in text and '星际舰队' in text and '**' not in text
assert pdf[0].get_fonts() and pdf[0].get_drawings()
assert len(fitz.open(root/'long.pdf'))>2
print('PASS independent DOCX/PPTX/XLSX readers, complete package relationships, Unicode/table/formula semantics, embedded PDF text and pagination')
