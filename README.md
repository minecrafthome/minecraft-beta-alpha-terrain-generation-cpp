# Minecraft@Home Template Repo

> This repo is used as a template during the creation of any new repositories in the @minecrafthome org

<!-- Mark an 'x' in the applicable boxes below, e.g. like [x] -->

## Process checklist

- [ ] On creation of a a new repo in the @minecrafthome GitHub organization, ensure this template is selected in the new repo screen. **Do not select a license, add a .gitignore, or initialize the repo with a readme on this screen.** 

> Once created, follow this checklist.

![New repo template](https://i.imgur.com/tTIJoOT.png)

### Repo Metadata

- [ ] Replace the description with a statement clearly representing the purpose of the new repo.
- [ ] If applicable, replace or remove the website URL.
- [ ] In addition to `minecraft-at-home`, ensure a handful of relevant topics are used which categorize the purpose, subject, and top technologies used.
- [ ] Topics which are not relevant to the new repo should be removed.

![Repo metadata](https://i.imgur.com/NXN01iE.png)

### Initialize git

- [ ] Clone the new repo to your local machine, initialize the repo with an empty commit, and push this change.

```bash
$ git clone git@github.com:minecrafthome/example.git
Cloning into 'example'...
warning: You appear to have cloned an empty repository.
$ git commit -m 'feat: initial commit' --allow-empty
[master (root-commit) abc1234] feat: initial commit
$ git push
$ git push
Enumerating objects, done.
Counting objects, done.
Delta compression using up to 8 threads
Compressing objects, done.
Writing objects, done.
To github.com:minecrafthome/example.git
 * [new branch]      master -> master
````

### Third-party app config

- [ ] Ensure [.github/semantic.yml](.github/semantic.yml) is [configured](https://github.com/zeke/semantic-pull-requests/blob/master/README.md) with the appropriate scopes for the new repo. Scopes must be lower-case and represent a single component. Some common scopes are already defined, such as `docs` for any readme or documentation changes and `ci` for any automated commits.

- [ ] Ensure [renovate.json](renovate.json) is [configured](https://docs.renovatebot.com/configuration-options/). For example, enable or configure any [managers](https://docs.renovatebot.com/modules/manager/) which may be required depending on the technologies used in the new repo.
