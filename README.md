# Tools
My tools for developing

1.PlistExtracter
  Need modify Urho3D PListFile for supporting rotation
```c++
//For .h
PListFile.h
     IntRect GetIntRect() const;
     ==>
     IntRect GetIntRect(bool rotate = false) const;

//For cpp
PListFile.cpp
    IntRect PListValue::GetIntRect() const
    {
        if (type_ != PLVT_STRING)
            return IntRect::ZERO;

        int x, y, w, h;
        sscanf(string_->CString(), "{{%d,%d},{%d,%d}}", &x, &y, &w, &h);    // NOLINT(cert-err34-c)
        return {x, y, x + w, y + h};
    }
    ==>
    IntRect PListValue::GetIntRect(bool rotate/* = false*/) const
    {
        if (type_ != PLVT_STRING)
            return IntRect::ZERO;

        int x, y, w, h;
        if (rotate)
            sscanf(string_->CString(), "{{%d,%d},{%d,%d}}", &x, &y, &h, &w);    // NOLINT(cert-err34-c)
        else
            sscanf(string_->CString(), "{{%d,%d},{%d,%d}}", &x, &y, &w, &h);    // NOLINT(cert-err34-c)
        return {x, y, x + w, y + h};
    }
```
