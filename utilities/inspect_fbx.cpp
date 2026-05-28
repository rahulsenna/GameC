#include "../libs/ufbx.c"
#include <stdio.h>

int main() {
    ufbx_error error;
    ufbx_scene *scene = ufbx_load_file("assets/Sophie.fbx", NULL, &error);
    if (!scene) {
        printf("Failed to load: %s\n", error.description.data);
        return 1;
    }
    
    printf("Loaded FBX. Materials: %zu\n", scene->materials.count);
    for (size_t i = 0; i < scene->materials.count; i++) {
        ufbx_material *mat = scene->materials.data[i];
        printf("Material: %s\n", mat->name.data);
        for (size_t j = 0; j < mat->textures.count; j++) {
            ufbx_material_texture mat_tex = mat->textures.data[j];
            printf("  - Prop: %s\n", mat_tex.material_prop.data);
            if (mat_tex.texture) {
                printf("    - Tex: %s (Type: %d)\n", mat_tex.texture->filename.data, mat_tex.texture->type);
            }
        }
    }
    
    ufbx_free_scene(scene);
    return 0;
}
