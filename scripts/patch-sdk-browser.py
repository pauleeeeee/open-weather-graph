#!/usr/bin/env python3
"""
Patch the Pebble SDK's browser.py so that when emu-app-config opens a config URL
that is a data URI (from our patched Clay), we inject the localhost return URL
into the HTML (replace $$$RETURN_TO$$$) before opening. That way Save redirects
to http://localhost:PORT/close?data and the SDK receives the config.

Run once after installing the pebble tool, and again if you reinstall/update it:
  python3 scripts/patch-sdk-browser.py

Finds pebble_tool.util.browser in the same Python env as 'pebble' or in common paths.
"""
import os
import sys

OLD_open_config_page = '''    def open_config_page(self, url, callback):
        self.port = port = self._choose_port()
        url = self.url_append_params(url, {'return_to': 'http://localhost:{}/close?'.format(port)})
        webbrowser.open_new(url)
        self.serve_page(port, callback)'''

NEW_open_config_page = '''    def open_config_page(self, url, callback):
        self.port = port = self._choose_port()
        return_to = 'http://localhost:{}/close?'.format(port)
        if url.startswith('data:'):
            # Data URI from Clay (emulator) - inject return_to into HTML so Save redirects to our server
            try:
                from six.moves.urllib.parse import unquote, quote
                if ',' in url:
                    header, encoded = url.split(',', 1)
                    html = unquote(encoded)
                    if '$$$RETURN_TO$$$' in html:
                        html = html.replace('$$$RETURN_TO$$$', return_to)
                        encoded = quote(html, safe='')
                    url = header + ',' + encoded
            except Exception:
                pass
        else:
            url = self.url_append_params(url, {'return_to': return_to})
        webbrowser.open_new(url)
        self.serve_page(port, callback)'''


def find_browser_py():
    # Try importing to find the package path
    try:
        import pebble_tool.util.browser as m
        return m.__file__
    except ImportError:
        pass
    # Common paths for uv/pip installs
    for base in [
        os.path.expanduser("~/.local/share/uv/tools/pebble-tool"),
        os.path.expanduser("~/.local/lib/python3.12/site-packages"),
        os.path.expanduser("~/.local/lib/python3.12/site-packages"),
    ]:
        path = os.path.join(base, "lib", "python3.12", "site-packages", "pebble_tool", "util", "browser.py")
        if os.path.isfile(path):
            return path
        path = os.path.join(base, "pebble_tool", "util", "browser.py")
        if os.path.isfile(path):
            return path
    return None


def main():
    path = find_browser_py()
    if not path:
        print("patch-sdk-browser: could not find pebble_tool.util.browser (install pebble tool first)", file=sys.stderr)
        sys.exit(1)

    with open(path, "r") as f:
        content = f.read()

    if "if url.startswith('data:'):" in content and "'$$$RETURN_TO$$$'" in content:
        print("patch-sdk-browser: browser.py already patched")
        return

    if OLD_open_config_page not in content:
        print("patch-sdk-browser: browser.py format changed, patch may need update", file=sys.stderr)
        sys.exit(1)

    content = content.replace(OLD_open_config_page, NEW_open_config_page)
    with open(path, "w") as f:
        f.write(content)
    print("patch-sdk-browser: patched", path)


if __name__ == "__main__":
    main()
