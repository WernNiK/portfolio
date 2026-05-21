const tabs = document.querySelectorAll('.about-tab');
const panels = document.querySelectorAll('.about-panel');
const aboutSection = document.querySelector('.about');
const aboutImage = document.querySelector('.about-image img');

const setAboutImage = (tab) => {
    if (!aboutImage || !tab) return;
    const nextSrc = tab.dataset.image;
    const nextAlt = tab.dataset.imageAlt;
    if (nextSrc) {
        aboutImage.src = nextSrc;
    }
    if (nextAlt) {
        aboutImage.alt = nextAlt;
    }
};

if (tabs.length && panels.length) {
    const activeTab = document.querySelector('.about-tab.is-active');
    setAboutImage(activeTab);

    tabs.forEach((tab) => {
        tab.addEventListener('click', () => {
            const target = tab.dataset.tab;

            tabs.forEach((t) => {
                t.classList.remove('is-active');
                t.setAttribute('aria-selected', 'false');
            });

            panels.forEach((panel) => {
                panel.classList.toggle('is-active', panel.dataset.panel === target);
            });

            tab.classList.add('is-active');
            tab.setAttribute('aria-selected', 'true');
            setAboutImage(tab);

            if (aboutSection) {
                aboutSection.classList.toggle('hide-image', target === 'experience');
            }
        });
    });
}
