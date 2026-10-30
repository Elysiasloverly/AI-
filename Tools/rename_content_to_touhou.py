"""旧命名脚本的兼容入口；统一从 Presentation/catalog.json 更新显示文案。"""
from pathlib import Path
import runpy, unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Tools/Presentation/update_assets.py'),run_name='__main__')
