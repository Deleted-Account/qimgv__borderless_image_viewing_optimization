# Push Policy

本项目是 qimgv 的 KDE 定制 fork。所有改动只推送到 `origin` 的 `kde-tweaks` 分支。

## 仓库与分支

- `origin`: `https://github.com/Deleted-Account/qimgv__Linux-KDE-Tweaks.git`
- 唯一推送目标分支：`kde-tweaks`
- `upstream` 是原作者仓库，只用于同步上游代码，不推送：

```text
upstream: https://github.com/easymodo/qimgv
```

## 必须遵守的推送规则

1. 本地工作在 `kde-tweaks` 分支上完成。
2. 推送命令始终是：

```bash
git push origin kde-tweaks
```

3. 不要推送到：
   - `master`
   - `main`
   - `upstream` 的任何分支
   - `--all`

4. 禁止使用以下命令：

```bash
git push
git push origin master
git push origin main
git push upstream <branch>
git push --all
```

原因：

- `master` 是历史基线分支，不是本 fork 的开发分支。
- 本仓库没有 `main` 分支。
- `upstream` 是只读上游，不能推送。
- `git push` 依赖当前分支/upstream 配置，容易误推到错误分支；显式写 `origin kde-tweaks` 最安全。

## 标准工作流

```bash
cd /home/luxxanna/Documents/__WIP__/_MyOwn_Projects_/__qimgv_fork__

git switch kde-tweaks
git pull --ff-only origin kde-tweaks

# 修改代码

git add <files>
git commit -m "KDE tweaks: <summary>"
git push origin kde-tweaks
```

## 新环境克隆后

```bash
git clone https://github.com/Deleted-Account/qimgv__Linux-KDE-Tweaks.git
cd qimgv__Linux-KDE-Tweaks

git switch -c kde-tweaks --track origin/kde-tweaks
```

之后仍然使用：

```bash
git push origin kde-tweaks
```

## 关于 `master`

是否删除远端 `master` 是独立决定，不影响本规则。

即使 `master` 仍然存在，也不得向它推送本项目的 KDE tweaks 改动。本文档约束的是推送行为，不依赖远端分支数量。

如果决定删除 `master`，顺序必须是：

1. 在 GitHub 仓库设置里把默认分支改为 `kde-tweaks`。
2. 删除远端 `master`。
3. 本地清理旧的跟踪分支。
