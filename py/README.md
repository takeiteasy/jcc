# PyJCC

> [!WARNING]
> Work in progress, I would not use this yet.

Python bindings for JCC

```bash
python3 setup.py install
```

```python
import PyJCC, sys;
vm=PyJCC.create();
PyJCC.load_stdlib(vm);
PyJCC.include(vm, 'include');
PyJCC.define(vm, 'TEST', '1');
PyJCC.undef(vm, 'TEST');
...
PyJCC.destroy(vm)
```

## LICENSE

see `../LICENSE`