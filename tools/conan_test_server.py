#!/usr/bin/env python3
"""Tiny Conan 2 remote for LAN test builds.

This is intentionally small and file-backed. It implements only the Conan 2
REST endpoints needed by `conan create`, `conan upload`, `conan install`, and
`conan download` for normal recipe/package artifacts.
"""

from __future__ import annotations

import argparse
import base64
import fnmatch
import hashlib
import hmac
import json
import os
import posixpath
import secrets
import shutil
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import BinaryIO
from urllib.parse import parse_qs, unquote, urlparse


CAPABILITIES = "revisions"
DEFAULT_USER = "ci"


class HttpError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status
        self.message = message


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def component(value: str) -> str:
    value = unquote(value)
    if not value or value in {".", ".."} or "/" in value or "\\" in value:
        raise HttpError(HTTPStatus.BAD_REQUEST, f"Invalid path component: {value!r}")
    return value


def safe_relpath(parts: list[str]) -> Path:
    if not parts:
        raise HttpError(HTTPStatus.BAD_REQUEST, "Missing file path")
    decoded = [unquote(part) for part in parts]
    if any(not part or part in {".", ".."} or "/" in part or "\\" in part for part in decoded):
        raise HttpError(HTTPStatus.BAD_REQUEST, "Invalid file path")
    return Path(*decoded)


def json_bytes(data: object) -> bytes:
    return json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")


@dataclass(frozen=True)
class RecipeKey:
    name: str
    version: str
    user: str
    channel: str

    def display(self) -> str:
        if self.user == "_" and self.channel == "_":
            return f"{self.name}/{self.version}"
        return f"{self.name}/{self.version}@{self.user}/{self.channel}"


@dataclass(frozen=True)
class PackageKey:
    recipe: RecipeKey
    rrev: str
    package_id: str


class FileStore:
    def __init__(self, root: Path):
        self.root = root.resolve()
        self.root.mkdir(parents=True, exist_ok=True)

    def _recipe_base(self, key: RecipeKey) -> Path:
        return self.root / "conans" / key.name / key.version / key.user / key.channel

    def _recipe_rev(self, key: RecipeKey, rrev: str) -> Path:
        return self._recipe_base(key) / "revisions" / rrev

    def _recipe_files(self, key: RecipeKey, rrev: str) -> Path:
        return self._recipe_rev(key, rrev) / "files"

    def _package_rev(self, key: PackageKey, prev: str) -> Path:
        return (
            self._recipe_rev(key.recipe, key.rrev)
            / "packages"
            / key.package_id
            / "revisions"
            / prev
        )

    def _package_files(self, key: PackageKey, prev: str) -> Path:
        return self._package_rev(key, prev) / "files"

    def _meta_path(self, rev_dir: Path) -> Path:
        return rev_dir / ".conan-test-server.json"

    def _ensure_meta(self, rev_dir: Path) -> None:
        rev_dir.mkdir(parents=True, exist_ok=True)
        meta = self._meta_path(rev_dir)
        if not meta.exists():
            meta.write_bytes(json_bytes({"time": now_iso()}))

    def _read_time(self, rev_dir: Path) -> str:
        meta = self._meta_path(rev_dir)
        if meta.exists():
            try:
                value = json.loads(meta.read_text(encoding="utf-8")).get("time")
                if value:
                    return str(value)
            except json.JSONDecodeError:
                pass
        return datetime.fromtimestamp(rev_dir.stat().st_mtime, timezone.utc).isoformat()

    def recipe_revisions(self, key: RecipeKey) -> list[dict[str, str]]:
        revisions_dir = self._recipe_base(key) / "revisions"
        if not revisions_dir.is_dir():
            return []
        entries = []
        for child in revisions_dir.iterdir():
            if child.is_dir():
                entries.append({"revision": child.name, "time": self._read_time(child)})
        return sorted(entries, key=lambda item: item["time"], reverse=True)

    def latest_recipe(self, key: RecipeKey) -> dict[str, str]:
        revisions = self.recipe_revisions(key)
        if not revisions:
            raise HttpError(HTTPStatus.NOT_FOUND, "Recipe revision not found")
        return revisions[0]

    def package_revisions(self, key: PackageKey) -> list[dict[str, str]]:
        revisions_dir = (
            self._recipe_rev(key.recipe, key.rrev)
            / "packages"
            / key.package_id
            / "revisions"
        )
        if not revisions_dir.is_dir():
            return []
        entries = []
        for child in revisions_dir.iterdir():
            if child.is_dir():
                entries.append({"revision": child.name, "time": self._read_time(child)})
        return sorted(entries, key=lambda item: item["time"], reverse=True)

    def latest_package(self, key: PackageKey) -> dict[str, str]:
        revisions = self.package_revisions(key)
        if not revisions:
            raise HttpError(HTTPStatus.NOT_FOUND, "Package revision not found")
        return revisions[0]

    def recipe_file_root(self, key: RecipeKey, rrev: str, create: bool = False) -> Path:
        root = self._recipe_files(key, rrev)
        if create:
            self._ensure_meta(self._recipe_rev(key, rrev))
            root.mkdir(parents=True, exist_ok=True)
        elif not root.is_dir():
            raise HttpError(HTTPStatus.NOT_FOUND, "Recipe files not found")
        return root

    def package_file_root(self, key: PackageKey, prev: str, create: bool = False) -> Path:
        root = self._package_files(key, prev)
        if create:
            self._ensure_meta(self._recipe_rev(key.recipe, key.rrev))
            self._ensure_meta(self._package_rev(key, prev))
            root.mkdir(parents=True, exist_ok=True)
        elif not root.is_dir():
            raise HttpError(HTTPStatus.NOT_FOUND, "Package files not found")
        return root

    def list_files(self, root: Path) -> dict[str, dict[str, int]]:
        files: dict[str, dict[str, int]] = {}
        for path in sorted(root.rglob("*")):
            if path.is_file() and not path.name.startswith(".conan-test-server"):
                rel = path.relative_to(root).as_posix()
                files[rel] = {"size": path.stat().st_size}
        return files

    def put_file(
        self,
        root: Path,
        relpath: Path,
        body: BinaryIO,
        size: int,
        expected_sha1: str | None,
    ) -> None:
        target = (root / relpath).resolve()
        if not str(target).startswith(str(root.resolve()) + os.sep):
            raise HttpError(HTTPStatus.BAD_REQUEST, "Invalid destination path")
        target.parent.mkdir(parents=True, exist_ok=True)
        tmp = target.with_name(f".{target.name}.{secrets.token_hex(8)}.tmp")
        digest = hashlib.sha1()
        remaining = size
        with tmp.open("wb") as out:
            while remaining > 0:
                chunk = body.read(min(1024 * 1024, remaining))
                if not chunk:
                    tmp.unlink(missing_ok=True)
                    raise HttpError(HTTPStatus.BAD_REQUEST, "Unexpected end of upload")
                remaining -= len(chunk)
                digest.update(chunk)
                out.write(chunk)
        actual_sha1 = digest.hexdigest()
        if expected_sha1 and not hmac.compare_digest(actual_sha1, expected_sha1.lower()):
            tmp.unlink(missing_ok=True)
            raise HttpError(HTTPStatus.BAD_REQUEST, "X-Checksum-Sha1 mismatch")
        tmp.replace(target)

    def iter_recipes(self) -> list[RecipeKey]:
        base = self.root / "conans"
        if not base.is_dir():
            return []
        result: list[RecipeKey] = []
        for name_dir in base.iterdir():
            if not name_dir.is_dir():
                continue
            for version_dir in name_dir.iterdir():
                if not version_dir.is_dir():
                    continue
                for user_dir in version_dir.iterdir():
                    if not user_dir.is_dir():
                        continue
                    for channel_dir in user_dir.iterdir():
                        if not channel_dir.is_dir():
                            continue
                        key = RecipeKey(name_dir.name, version_dir.name, user_dir.name, channel_dir.name)
                        if self.recipe_revisions(key):
                            result.append(key)
        return sorted(result, key=lambda key: key.display())

    def search_packages(self, recipe: RecipeKey, rrev: str | None) -> dict[str, dict[str, str]]:
        if rrev is None:
            rrev = self.latest_recipe(recipe)["revision"]
        packages_dir = self._recipe_rev(recipe, rrev) / "packages"
        if not packages_dir.is_dir():
            return {}
        result: dict[str, dict[str, str]] = {}
        for package_dir in sorted(packages_dir.iterdir()):
            if not package_dir.is_dir():
                continue
            key = PackageKey(recipe, rrev, package_dir.name)
            revisions = self.package_revisions(key)
            if not revisions:
                continue
            latest_prev = revisions[0]["revision"]
            info = self._package_files(key, latest_prev) / "conaninfo.txt"
            if info.exists():
                result[package_dir.name] = {"content": info.read_text(encoding="utf-8", errors="replace")}
            else:
                result[package_dir.name] = {}
        return result

    def remove_recipe(self, recipe: RecipeKey, rrev: str | None = None) -> None:
        target = self._recipe_base(recipe) if rrev is None else self._recipe_rev(recipe, rrev)
        if not target.exists():
            raise HttpError(HTTPStatus.NOT_FOUND, "Recipe not found")
        shutil.rmtree(target)

    def remove_package(self, key: PackageKey, prev: str | None = None) -> None:
        package_root = self._recipe_rev(key.recipe, key.rrev) / "packages" / key.package_id
        target = package_root if prev is None else package_root / "revisions" / prev
        if not target.exists():
            raise HttpError(HTTPStatus.NOT_FOUND, "Package not found")
        shutil.rmtree(target)


class ConanHandler(BaseHTTPRequestHandler):
    server_version = "ConanTestServer/0.1"

    @property
    def app(self) -> "ConanServer":
        return self.server  # type: ignore[return-value]

    def log_message(self, fmt: str, *args: object) -> None:
        if self.app.quiet:
            return
        sys.stderr.write("%s - - [%s] %s\n" % (self.client_address[0], self.log_date_time_string(), fmt % args))

    def do_GET(self) -> None:
        self._dispatch("GET")

    def do_HEAD(self) -> None:
        self._dispatch("HEAD")

    def do_PUT(self) -> None:
        self._dispatch("PUT")

    def do_DELETE(self) -> None:
        self._dispatch("DELETE")

    def _send_bytes(
        self,
        status: int,
        data: bytes = b"",
        content_type: str = "text/plain; charset=utf-8",
        extra_headers: dict[str, str] | None = None,
    ) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        for key, value in (extra_headers or {}).items():
            self.send_header(key, value)
        self.end_headers()
        if self.command != "HEAD" and data:
            self.wfile.write(data)

    def _send_json(self, status: int, data: object) -> None:
        self._send_bytes(status, json_bytes(data), "application/json")

    def _send_file(self, path: Path) -> None:
        if not path.is_file():
            raise HttpError(HTTPStatus.NOT_FOUND, "File not found")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(path.stat().st_size))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if self.command != "HEAD":
            with path.open("rb") as src:
                shutil.copyfileobj(src, self.wfile)

    def _dispatch(self, method: str) -> None:
        try:
            parsed = urlparse(self.path)
            path = posixpath.normpath(parsed.path)
            if parsed.path.endswith("/") and not path.endswith("/"):
                path += "/"
            parts = [part for part in path.split("/") if part]
            query = parse_qs(parsed.query)

            if method == "GET" and parts == ["v1", "ping"]:
                self._send_bytes(
                    HTTPStatus.OK,
                    b"OK",
                    extra_headers={"X-Conan-Server-Capabilities": CAPABILITIES},
                )
                return

            if parts[:2] == ["v2", "users"]:
                self._handle_users(method, parts[2:])
                return

            if parts[:2] != ["v2", "conans"]:
                raise HttpError(HTTPStatus.NOT_FOUND, "Unknown endpoint")

            self._check_bearer_if_required()

            if parts == ["v2", "conans", "search"]:
                if method != "GET":
                    raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
                self._handle_recipe_search(query)
                return

            self._handle_conans(method, parts[2:], query)
        except HttpError as exc:
            self._send_bytes(exc.status, exc.message.encode("utf-8"))
        except Exception as exc:  # noqa: BLE001
            self.log_error("Unhandled error: %r", exc)
            self._send_bytes(HTTPStatus.INTERNAL_SERVER_ERROR, str(exc).encode("utf-8"))

    def _handle_users(self, method: str, tail: list[str]) -> None:
        if method != "GET":
            raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
        if tail == ["authenticate"]:
            user, password = self._basic_auth()
            token = self.app.authenticate(user, password)
            self._send_bytes(HTTPStatus.OK, token.encode("utf-8"))
            return
        if tail == ["check_credentials"]:
            self._check_bearer_if_required(force=True)
            self._send_bytes(HTTPStatus.OK, b"OK")
            return
        raise HttpError(HTTPStatus.NOT_FOUND, "Unknown user endpoint")

    def _basic_auth(self) -> tuple[str, str]:
        header = self.headers.get("Authorization", "")
        if not header.startswith("Basic "):
            raise HttpError(HTTPStatus.UNAUTHORIZED, "Basic authentication required")
        try:
            decoded = base64.b64decode(header.split(" ", 1)[1]).decode("utf-8")
            user, password = decoded.split(":", 1)
        except Exception as exc:  # noqa: BLE001
            raise HttpError(HTTPStatus.UNAUTHORIZED, "Invalid basic authentication") from exc
        return user, password

    def _check_bearer_if_required(self, force: bool = False) -> None:
        if not self.app.password:
            return
        header = self.headers.get("Authorization", "")
        if not header.startswith("Bearer "):
            raise HttpError(HTTPStatus.UNAUTHORIZED, "Bearer token required")
        token = header.split(" ", 1)[1]
        if token not in self.app.tokens:
            raise HttpError(HTTPStatus.UNAUTHORIZED, "Invalid bearer token")

    def _handle_recipe_search(self, query: dict[str, list[str]]) -> None:
        pattern = query.get("q", ["*"])[0] or "*"
        ignorecase = query.get("ignorecase", ["True"])[0].lower() != "false"
        if ignorecase:
            pattern_cmp = pattern.lower()
        else:
            pattern_cmp = pattern
        matches = []
        for key in self.app.store.iter_recipes():
            display = key.display()
            display_cmp = display.lower() if ignorecase else display
            if fnmatch.fnmatchcase(display_cmp, pattern_cmp):
                matches.append(display)
        self._send_json(HTTPStatus.OK, {"results": matches})

    def _handle_conans(self, method: str, parts: list[str], query: dict[str, list[str]]) -> None:
        if len(parts) < 4:
            raise HttpError(HTTPStatus.NOT_FOUND, "Recipe endpoint is incomplete")
        recipe = RecipeKey(*(component(part) for part in parts[:4]))
        tail = parts[4:]

        if tail == ["latest"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            self._send_json(HTTPStatus.OK, self.app.store.latest_recipe(recipe))
            return

        if tail == ["revisions"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            self._send_json(HTTPStatus.OK, {"revisions": self.app.store.recipe_revisions(recipe)})
            return

        if tail == ["search"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            self._send_json(HTTPStatus.OK, self.app.store.search_packages(recipe, None))
            return

        if len(tail) >= 2 and tail[0] == "revisions":
            rrev = component(tail[1])
            rest = tail[2:]
            self._handle_recipe_revision(method, recipe, rrev, rest, query)
            return

        raise HttpError(HTTPStatus.NOT_FOUND, "Unknown recipe endpoint")

    def _handle_recipe_revision(
        self,
        method: str,
        recipe: RecipeKey,
        rrev: str,
        tail: list[str],
        query: dict[str, list[str]],
    ) -> None:
        if not tail:
            if method == "DELETE":
                self.app.store.remove_recipe(recipe, rrev)
                self._send_bytes(HTTPStatus.OK, b"OK")
                return
            raise HttpError(HTTPStatus.NOT_FOUND, "Unknown recipe revision endpoint")

        if tail == ["files"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            root = self.app.store.recipe_file_root(recipe, rrev)
            self._send_json(HTTPStatus.OK, {"files": self.app.store.list_files(root)})
            return

        if tail[0] == "files":
            relpath = safe_relpath(tail[1:])
            if method in {"GET", "HEAD"}:
                root = self.app.store.recipe_file_root(recipe, rrev)
                self._send_file(root / relpath)
                return
            if method == "PUT":
                root = self.app.store.recipe_file_root(recipe, rrev, create=True)
                self._receive_file(root, relpath)
                self._send_bytes(HTTPStatus.CREATED, b"Created")
                return
            raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")

        if tail == ["search"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            self._send_json(HTTPStatus.OK, self.app.store.search_packages(recipe, rrev))
            return

        if tail[0] == "packages":
            self._handle_packages(method, recipe, rrev, tail[1:], query)
            return

        raise HttpError(HTTPStatus.NOT_FOUND, "Unknown recipe revision endpoint")

    def _handle_packages(
        self,
        method: str,
        recipe: RecipeKey,
        rrev: str,
        tail: list[str],
        query: dict[str, list[str]],
    ) -> None:
        if not tail:
            if method == "DELETE":
                self.app.store.remove_recipe(recipe, rrev)
                self._send_bytes(HTTPStatus.OK, b"OK")
                return
            raise HttpError(HTTPStatus.NOT_FOUND, "Package endpoint is incomplete")

        package = PackageKey(recipe, rrev, component(tail[0]))
        rest = tail[1:]

        if rest == ["latest"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            self._send_json(HTTPStatus.OK, self.app.store.latest_package(package))
            return

        if rest == ["revisions"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            self._send_json(HTTPStatus.OK, {"revisions": self.app.store.package_revisions(package)})
            return

        if len(rest) >= 2 and rest[0] == "revisions":
            prev = component(rest[1])
            self._handle_package_revision(method, package, prev, rest[2:])
            return

        raise HttpError(HTTPStatus.NOT_FOUND, "Unknown package endpoint")

    def _handle_package_revision(
        self,
        method: str,
        package: PackageKey,
        prev: str,
        tail: list[str],
    ) -> None:
        if not tail:
            if method == "DELETE":
                self.app.store.remove_package(package, prev)
                self._send_bytes(HTTPStatus.OK, b"OK")
                return
            raise HttpError(HTTPStatus.NOT_FOUND, "Unknown package revision endpoint")

        if tail == ["files"]:
            if method != "GET":
                raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")
            root = self.app.store.package_file_root(package, prev)
            self._send_json(HTTPStatus.OK, {"files": self.app.store.list_files(root)})
            return

        if tail[0] == "files":
            relpath = safe_relpath(tail[1:])
            if method in {"GET", "HEAD"}:
                root = self.app.store.package_file_root(package, prev)
                self._send_file(root / relpath)
                return
            if method == "PUT":
                root = self.app.store.package_file_root(package, prev, create=True)
                self._receive_file(root, relpath)
                self._send_bytes(HTTPStatus.CREATED, b"Created")
                return
            raise HttpError(HTTPStatus.METHOD_NOT_ALLOWED, "Method not allowed")

        raise HttpError(HTTPStatus.NOT_FOUND, "Unknown package revision endpoint")

    def _receive_file(self, root: Path, relpath: Path) -> None:
        length = self.headers.get("Content-Length")
        if length is None:
            raise HttpError(HTTPStatus.LENGTH_REQUIRED, "Content-Length required")
        try:
            size = int(length)
        except ValueError as exc:
            raise HttpError(HTTPStatus.BAD_REQUEST, "Invalid Content-Length") from exc
        expected_sha1 = self.headers.get("X-Checksum-Sha1")
        self.app.store.put_file(root, relpath, self.rfile, size, expected_sha1)


class ConanServer(ThreadingHTTPServer):
    def __init__(
        self,
        server_address: tuple[str, int],
        store: FileStore,
        user: str,
        password: str,
        quiet: bool,
    ):
        super().__init__(server_address, ConanHandler)
        self.store = store
        self.user = user
        self.password = password
        self.quiet = quiet
        self.tokens: set[str] = set()

    def authenticate(self, user: str, password: str) -> str:
        if self.password:
            if user != self.user or not hmac.compare_digest(password, self.password):
                raise HttpError(HTTPStatus.UNAUTHORIZED, "Wrong user or password")
        token = secrets.token_urlsafe(32)
        self.tokens.add(token)
        return token


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a tiny file-backed Conan 2 test remote.")
    parser.add_argument("--host", default=os.environ.get("CONAN_TEST_SERVER_HOST", "0.0.0.0"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("CONAN_TEST_SERVER_PORT", "9300")))
    parser.add_argument(
        "--storage",
        default=os.environ.get("CONAN_TEST_SERVER_STORAGE", ".conan-test-server/storage"),
        help="Directory for recipes and packages.",
    )
    parser.add_argument("--user", default=os.environ.get("CONAN_TEST_SERVER_USER", DEFAULT_USER))
    parser.add_argument(
        "--password",
        default=os.environ.get("CONAN_TEST_SERVER_PASSWORD", ""),
        help="Optional upload/download password. Empty means anonymous read/write.",
    )
    parser.add_argument("--quiet", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    store = FileStore(Path(args.storage))
    server = ConanServer((args.host, args.port), store, args.user, args.password, args.quiet)
    host, port = server.server_address
    auth_mode = f"auth user={args.user!r}" if args.password else "anonymous read/write"
    print(f"Conan test server listening on http://{host}:{port} ({auth_mode})")
    print(f"Storage: {store.root}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping Conan test server")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
