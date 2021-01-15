This is [Karbo2](https://github.com/Karbovanets/karbo/) version of Karbo Classic Wallet intended to work with Karbo Core starting from v. 2 (v. 2.3.0). 

**1. Clone wallet sources**

```
git clone https://github.com/Karbovanets/karbowanecwallet.git
```

**2. Set symbolic link to coin sources at the same level as `src`. For example:**

```
ln -s ../karbo cryptonote
```

or clone coins souces into `cryptonote` folder:

```
git clone https://github.com/Karbovanets/karbo.git cryptonote
```

Alternative way is to create git submodule:

```
git submodule add https://github.com/Karbovanets/karbo.git cryptonote
```

**3. Build**

```
mkdir build && cd build && cmake .. && make
```
