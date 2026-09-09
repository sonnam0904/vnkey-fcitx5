#!/usr/bin/env python3
"""Generate DEP-11 catalog metadata for a suite of the Telebit APT repo.

Why this exists
---------------
A .deb that ships /usr/share/metainfo is still invisible in every software
centre, because centres do not read packages — they read the *catalog* their
package manager downloaded. COSMIC Store, specifically, scans
{/usr/share,/var/lib,/var/cache}/{swcatalog,app-info}/{xml,yaml}, and apt fills
those from `dists/<suite>/<component>/dep11/` (see the DEP-11 index targets in
/etc/apt/apt.conf.d/50appstream, shipped by the `appstream` package). No dep11
directory in the repo means no entry in the store, however correct the package.

So this reads the metainfo out of the built .debs and writes, into
`dists/<suite>/main/dep11/`:

    Components-amd64.yml.gz          the catalog
    icons-{48x48,64x64,64x64@2,128x128}.tar.gz   rasterized icons

Two details are not obvious and are load-bearing:

* `Package:` comes from the .deb, not from the metainfo, and it is what the
  store's install button acts on. `appstreamcli compose` cannot supply it —
  it works on directory trees and knows nothing about packages — which is why
  the catalog XML is assembled here and only the *conversion* is handed to
  appstreamcli.
* Icons are resolved by the store as `<icons-dir>/<Origin>/<size>/<name>`
  (cosmic-store's appstream_cache.rs), from the icon tarballs — not from the
  hicolor theme in the package. An entry whose icon is missing from these
  tarballs renders blank no matter what the .deb installs.
"""

from __future__ import annotations

import argparse
import gzip
import shutil
import subprocess
import sys
import tarfile
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

# The icon sizes apt fetches (see the DEP-11 index targets in 50appstream).
#
# Each entry is (directory label, logical size, scale, rendered pixels). The @2
# variant is the distinction that matters: its file is 128px, but the catalog
# must declare the *logical* size 64 with scale 2, because a store rebuilds the
# directory name from those fields — cosmic-store formats "{w}x{h}@{scale}", so
# declaring 128x128 scale 2 would send it looking in a 128x128@2 directory that
# no tarball creates. Ubuntu's own catalogs use the same convention.
#
# 128x128@2 exists in the index too, but Ubuntu leaves it out and so do we
# rather than doubling the payload.
ICON_SIZES = [
    ("48x48", 48, 1, 48),
    ("64x64", 64, 1, 64),
    ("64x64@2", 64, 2, 128),
    ("128x128", 128, 1, 128),
]


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kwargs)


def deb_field(deb: Path, field: str) -> str:
    return run(["dpkg-deb", "-f", str(deb), field]).stdout.strip()


def rasterize(svg: Path, out: Path, size: int) -> bool:
    """SVG -> PNG. rsvg-convert first; ImageMagick is the fallback."""
    if shutil.which("rsvg-convert"):
        run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(out), str(svg)])
        return True
    if shutil.which("convert"):
        run(["convert", "-background", "none", "-resize", f"{size}x{size}", str(svg), str(out)])
        return True
    return False


def component_from_metainfo(path: Path) -> ET.Element | None:
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as err:
        print(f"WARNING: {path.name}: {err}", file=sys.stderr)
        return None
    return root if root.tag == "component" else None


XML_LANG = "{http://www.w3.org/XML/1998/namespace}lang"


def split_description(description: ET.Element) -> list[ET.Element]:
    """Regroup a metainfo <description> into per-language catalog ones.

    This is the one place the two AppStream formats genuinely disagree.
    Metainfo marks up translations inside the description —

        <description><p>English</p><p xml:lang="vi">Vietnamese</p></description>

    — while a catalog wants one description per language:

        <description><p>English</p></description>
        <description xml:lang="vi"><p>Vietnamese</p></description>

    Feeding the metainfo shape to `appstreamcli convert` is silently accepted
    and produces a C description with the Vietnamese paragraphs embedded in it
    as literal markup, which is what every store would then display.
    """
    by_lang: dict[str, list[ET.Element]] = {}

    for child in description:
        tag = child.tag
        if tag in ("p",):
            lang = child.get(XML_LANG, "C")
            copy = ET.Element(tag)
            copy.text = child.text
            copy.extend(list(child))
            by_lang.setdefault(lang, []).append(copy)
        elif tag in ("ul", "ol"):
            # List items carry the language, so one list can hold several.
            items_by_lang: dict[str, list[ET.Element]] = {}
            for item in child:
                lang = item.get(XML_LANG, "C")
                copy = ET.Element(item.tag)
                copy.text = item.text
                items_by_lang.setdefault(lang, []).append(copy)
            for lang, items in items_by_lang.items():
                container = ET.Element(tag)
                container.extend(items)
                by_lang.setdefault(lang, []).append(container)

    out = []
    # C first, so the untranslated reading is the one a parser meets first.
    for lang in sorted(by_lang, key=lambda value: (value != "C", value)):
        element = ET.Element("description")
        if lang != "C":
            element.set(XML_LANG, lang)
        element.extend(by_lang[lang])
        out.append(element)
    return out


def split_by_child_lang(container: ET.Element) -> list[ET.Element]:
    """Same regrouping as split_description, for <keywords>.

    Metainfo tags each keyword; a catalog tags the whole list. Left alone, the
    Vietnamese keywords end up in the C list, where they are dead weight for
    an English search and missing from a Vietnamese one.
    """
    by_lang: dict[str, list[ET.Element]] = {}
    for child in container:
        lang = child.get(XML_LANG, "C")
        copy = ET.Element(child.tag)
        copy.text = child.text
        by_lang.setdefault(lang, []).append(copy)

    out = []
    for lang in sorted(by_lang, key=lambda value: (value != "C", value)):
        element = ET.Element(container.tag)
        if lang != "C":
            element.set(XML_LANG, lang)
        element.extend(by_lang[lang])
        out.append(element)
    return out


def catalog_component(component: ET.Element, package: str, icon_name: str | None) -> ET.Element:
    """Turn a metainfo component into a catalog one.

    The two formats differ in exactly the parts that matter here: a catalog
    component names its package and carries cached icon references, and it has
    no use for the <launchable>/<content_rating> style tags that only describe
    the upstream project. Dropping the rest would be premature — the centres
    display description, categories, keywords and urls straight from this.
    """
    out = ET.Element("component", component.attrib)
    for child in component:
        if child.tag == "description":
            for translated in split_description(child):
                out.append(translated)
        elif child.tag == "keywords":
            for translated in split_by_child_lang(child):
                out.append(translated)
        else:
            out.append(child)

    pkgname = ET.SubElement(out, "pkgname")
    pkgname.text = package

    if icon_name is not None:
        # Cached icons override any <icon type="stock"> the metainfo had: the
        # store looks for a file, and this is the file it will find.
        for existing in out.findall("icon"):
            out.remove(existing)
        for _label, logical, scale, _pixels in ICON_SIZES:
            icon = ET.SubElement(out, "icon", {"type": "cached"})
            icon.set("width", str(logical))
            icon.set("height", str(logical))
            if scale != 1:
                icon.set("scale", str(scale))
            icon.text = icon_name
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repo_dir", type=Path, help="root of the APT repo checkout")
    parser.add_argument("suite", help="suite/codename, e.g. noble")
    parser.add_argument("debs", nargs="+", type=Path, help=".deb files of this suite")
    parser.add_argument(
        "--origin",
        default=None,
        help="AppStream origin; defaults to telebit-<suite>-main. This string is also the "
        "directory name the store looks for icons under, so changing it orphans the icons "
        "of catalogs already published.",
    )
    args = parser.parse_args()

    origin = args.origin or f"telebit-{args.suite}-main"
    dep11_dir = args.repo_dir / "dists" / args.suite / "main" / "dep11"

    catalog = ET.Element("components", {"version": "0.14", "origin": origin})
    # size label -> list of (arcname, file) to put in that size's tarball
    icons: dict[str, list[tuple[str, Path]]] = {label: [] for label, _, _, _ in ICON_SIZES}

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        for index, deb in enumerate(args.debs):
            if not deb.is_file():
                print(f"WARNING: {deb} is not a file, skipping", file=sys.stderr)
                continue

            package = deb_field(deb, "Package")
            root = tmpdir / f"deb{index}"
            root.mkdir()
            run(["dpkg-deb", "-x", str(deb), str(root)])

            metainfo_dir = root / "usr/share/metainfo"
            if not metainfo_dir.is_dir():
                # The metapackage has no metadata of its own, by design.
                continue

            for metainfo in sorted(metainfo_dir.glob("*.xml")):
                component = component_from_metainfo(metainfo)
                if component is None:
                    continue

                cid_element = component.find("id")
                cid = cid_element.text.strip() if cid_element is not None and cid_element.text else ""
                if not cid:
                    print(f"WARNING: {metainfo.name} has no <id>, skipping", file=sys.stderr)
                    continue

                icon_name = None
                svg = root / f"usr/share/icons/hicolor/scalable/apps/{cid}.svg"
                if svg.is_file():
                    icon_name = f"{package}_{cid}.png"
                    for label, _logical, _scale, pixels in ICON_SIZES:
                        png = tmpdir / f"{label.replace('@', '_')}_{icon_name}"
                        if rasterize(svg, png, pixels):
                            icons[label].append((icon_name, png))
                        else:
                            print(
                                "ERROR: no rsvg-convert and no ImageMagick; cannot rasterize "
                                f"{svg.name}. Install librsvg2-bin.",
                                file=sys.stderr,
                            )
                            return 1
                    if not icons["64x64"]:
                        icon_name = None

                catalog.append(catalog_component(component, package, icon_name))
                print(f"{deb.name}: {cid}" + (" (with icon)" if icon_name else ""))

        if len(catalog) == 0:
            print("ERROR: none of the .debs carried AppStream metadata", file=sys.stderr)
            return 1

        dep11_dir.mkdir(parents=True, exist_ok=True)

        # The catalog is written as XML and converted, rather than emitted as
        # YAML directly: DEP-11 has rules about which fields are folded, how
        # descriptions are marked up and how translations are keyed, and
        # appstreamcli is the reference implementation of them.
        xml_path = tmpdir / "catalog.xml"
        ET.ElementTree(catalog).write(xml_path, encoding="utf-8", xml_declaration=True)
        yml_path = tmpdir / "Components-amd64.yml"
        try:
            run(["appstreamcli", "convert", str(xml_path), str(yml_path)])
        except subprocess.CalledProcessError as err:
            print(f"ERROR: appstreamcli convert failed:\n{err.stderr}", file=sys.stderr)
            return 1

        # Both the plain and the gzipped file, because apt's index target is
        # the *uncompressed* name (Components-<arch>.yml in 50appstream) and it
        # only fetches a target whose base name it can find in the Release hash
        # list. Publishing .gz alone means apt never asks for the catalog at
        # all — no error, no entry in any store. Ubuntu's own repos list the
        # plain file alongside .gz and .xz for the same reason.
        plain = dep11_dir / "Components-amd64.yml"
        shutil.copyfile(yml_path, plain)
        gz = dep11_dir / "Components-amd64.yml.gz"
        with open(yml_path, "rb") as src, gzip.open(gz, "wb", compresslevel=9) as dst:
            shutil.copyfileobj(src, dst)
        print(f"wrote {plain} and {gz}")

        for label, _logical, _scale, _pixels in ICON_SIZES:
            if not icons[label]:
                # Leave a stale tarball behind rather than an empty one: apt
                # treats a missing icon index as "no icons", and an empty one
                # as "the icons are gone".
                continue
            # Plain .tar next to .tar.gz, same reason as the catalog above:
            # 50appstream's icon targets are named `icons-<size>.tar`.
            tar_path = dep11_dir / f"icons-{label}.tar"
            with tarfile.open(tar_path, "w") as tar:
                for arcname, png in icons[label]:
                    tar.add(png, arcname=arcname)
            with open(tar_path, "rb") as src, gzip.open(
                dep11_dir / f"icons-{label}.tar.gz", "wb", compresslevel=9
            ) as dst:
                shutil.copyfileobj(src, dst)
            print(f"wrote {tar_path} and {tar_path}.gz")

    return 0


if __name__ == "__main__":
    sys.exit(main())
